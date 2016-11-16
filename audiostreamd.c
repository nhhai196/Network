/* audiostreamd.c
* @author Hai Nguyen 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <errno.h>
//Generic socket:

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

//Generic system:

#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/signal.h>

//Protocol specific socket:

#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define BUFFSIZE 1000

// Global variables
int a = 1000; 
int delta = 0.5; // Method B
int epsilon = 0.1; // Method C

struct ccvars{
	int tau; // time spacing between packets
	int tbl; // target buffer level 
	int cbl; // current buffer level 
	int payload_size;
	int mode; 
	int gamma; // playback rate
	int a; 
	int sd_to_send; // socket to send message
	int sd_to_rcv; // socket for receiving message
	unsigned long cli_ip; // client IP
} ;

int addr_len;
struct ccvars cc; 
long total_bytes_sent;

//Function declarations
void SIGPOLLHandler();
void read_feedback(char buffer[]);
void update_throughput();
void initialize();
void handle_single_client(char pathname[], int serv_port, int cli_port, char * logs);
void packet_sleep();

int main(int argc, char * argv[]){
	// Variable declrations and initializations
	int udp_sd, tcp_sd;
	int tcp_port, new_tcp_sd;
	int serv_udp_port;
	int cli_udp_port;
	struct sockaddr_in tserv_add, userv_add, tcli_add, ucli_add;
	char buffer[BUFFSIZE];
	int child_count = 0;
	int processID, n;

	// get the size of struct addr
	addr_len = sizeof(tserv_add);

	// Check whether the number of input arguments is correct
	if (argc != 7){
		printf("Usage: audiostreamd tcp-port udp-port payload-size packet-spacing mode logfile-s\n");
		exit(0);
	}
	
	// Convert Inputs
	tcp_port = atoi(argv[1]);
	serv_udp_port = atoi(argv[2]);
	cc.payload_size = atoi(argv[3]);
	cc.tau = 1000* atoi(argv[4]); // micro seconds
	cc.mode = atoi(argv[5]);
	
	printf("tcp port: %d\n", tcp_port);
	
	//Create a TCP socket 
	tcp_sd = socket(AF_INET, SOCK_STREAM, 0);

	if (tcp_sd < 0) {
		printf("ERROR opening socket\n");
		exit(1);
	}
	
	// Initialize socket struct
	bzero((char *) &tserv_add, sizeof(tserv_add));

	tserv_add.sin_family = AF_INET;
	tserv_add.sin_port = htons(tcp_port);
	tserv_add.sin_addr.s_addr = INADDR_ANY;
	bzero(&(tserv_add.sin_zero), 8);

	// Bind the host address using bind() 
	if (bind(tcp_sd, (struct sockaddr *) &tserv_add, sizeof(tserv_add)) < 0){
		printf("ERROR on binding TCP\n");  
		exit(1);
	}

	// Start listening for the clients
	listen(tcp_sd, 5);
	
	while(1) {
		//printf("Waiting for data...");
		new_tcp_sd = accept(tcp_sd, (struct sockaddr *) &tcli_add, &addr_len);
		
		// Save client address
		cc.cli_ip = tcli_add.sin_addr.s_addr;
		
		if (new_tcp_sd < 0) {
			printf("ERROR on accept\n");
		}
		
		// read the request from the client  
		if ((n = read(new_tcp_sd, buffer, BUFFSIZE)) < 0){
			perror("ERROR on read tcp request");
		}
		
		//char * port_num = (char *) malloc(10);
		char pathname[100];
		buffer[n] = '\0';
		//printf("Buffer %s\n", buffer);
		cli_udp_port = atoi(strtok(buffer, " "));
		strcpy(pathname, strtok(NULL, " "));
		
		printf("path name %s, port %d\n", pathname, cli_udp_port);
		
		//Check if the file requested exists
		if( access( pathname, F_OK ) == -1 ) {
			// file doesn't exists
			perror("ERROR on file requested");
			// Send a negative message "KO" on TCP to the client
			if (write(new_tcp_sd, "KO", 2) < 0){
				perror("ERROR on send");
			}
		}
		// file exist
		memset(buffer, 0, BUFFSIZE);
		sprintf(buffer, "OK %d", serv_udp_port);
		if (write(new_tcp_sd, buffer, strlen(buffer)) < 0){
			perror("ERROR on send");
		}
		
		printf("Check before fork\n");
		// Create child process
		int pid = fork();

		if (pid <0) {
			printf("ERROR on fork\n");
			exit(1);
		}
		
		child_count++;
		if (pid == 0) { // Child process
			printf("Check in child\n");
			handle_single_client(pathname, serv_udp_port, cli_udp_port, argv[6]);
			printf("Child done\n");
			exit(0);
		}
		// Parent process
		close(new_tcp_sd);
		while (child_count){
			processID = waitpid((pid_t) - 1, NULL, WNOHANG); 		
			if (processID ==0){
				break; // No zombie to wait
			}
			else if (processID > 0){
				child_count--;
			}
		}
	}
	
}

void SIGPOLLHandler(int sig){
	//printf("SIGPOLL\n");
	int numbytes;
	int size = cc.payload_size;
	do{
		char buffer[size];
		struct sockaddr_in serv_add;
		memset(buffer, 0, size);
		memset((char *) &serv_add, 0, addr_len);
	
		// Get the feedback packet
		numbytes = recvfrom(cc.sd_to_rcv, buffer, size, 0, (struct sockaddr*) &serv_add, &addr_len);
		if (numbytes < 0){
			if (errno != EWOULDBLOCK){
				printf("ERROR %d on recvfrom\n", errno);
			}
		}
		if (numbytes > 0){
			printf("Received a feedback packet: %s\n", buffer);
			read_feedback(buffer);
			update_throughput();
		}
	} while (numbytes > 0);
}

void update_throughput(){
	if (cc.mode ==1){
		if (cc.cbl < cc.tbl){ // Q(t) < Q* 
			cc.tau = cc.tau - a;
			if (cc.tau < 0){
				cc.tau = 1;
			}
		}
	
		if (cc.cbl > cc.tbl){ // Q(t) > Q*
			cc.tau = cc.tau + a;
		}
	}
	else if (cc.mode == 2){
		if (cc.cbl < cc.tbl){
			cc.tau = cc.tau - a;
			if (cc.tau <= 0){
				cc.tau = 1;
			}
		}
	
		if (cc.cbl > cc.tbl){
			cc.tau = cc.tau/delta;
		}	
	}
	else if (cc.mode == 3){
		cc.tau -= epsilon * (cc.tbl - cc.cbl);
	}
	else if (cc.mode == 4){
		cc.tau = cc.tau - epsilon * (cc.tbl - cc.cbl);
	}
	else {
		printf("ERROR on mode\n");
	}
}

void read_feedback(char buffer[]){
	char * ptr = strtok(buffer, " ");
	ptr = strtok(NULL, " ");
	cc.cbl = atoi(ptr);
	ptr = strtok(NULL, " ");
	cc.tbl = atoi(ptr);
	ptr = strtok(NULL, " ");
	cc.gamma = atoi(ptr);
}

void packet_sleep(int tau){
	struct timespec start, remain;
	start.tv_sec = 0; 
	start.tv_nsec = 1000 * tau; 
	while (nanosleep(&start,&remain) == -1){	
		start = remain;
		remain.tv_sec = 0;
		remain.tv_nsec = 0;
		//printf("Interrupted by signal\n");
	}  		
}

// Function to compute elapsed time in seconds
double elapsed_time(struct timeval start, struct timeval end){
  double result, sec, usec;
  sec = (end.tv_sec - start.tv_sec);	
  usec = (end.tv_usec - start.tv_usec);
  //printf("sec is %lld, usec is %lld\n", sec, usec);
  result =  sec + usec/1000000; 
	   
  return result;
}

void handle_single_client(char pathname[], int serv_port, int cli_port, char * logs){
	int size = cc.payload_size;
	struct sockaddr_in my_addr, their_addr;
	struct timeval start, end;
	double elapsed; 
	char buf[size+1];
	char * log = (char*) malloc(10000000);
	
	memset(log, 0, 10000000);
	printf("Check afer log\n");
	// Create a UDP socket for receiving the feedback
	if ((cc.sd_to_rcv = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("ERROR on creating socket");
		exit(1);
	}

	// zero out the structure
	memset((char *) &my_addr, 0, addr_len);

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(serv_port);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bzero(&(my_addr.sin_zero), 8); // zero the rest of the struct

	// bind socket to port 
	if (bind(cc.sd_to_rcv, (struct sockaddr *) &my_addr, addr_len) < 0){
		perror("ERROR on binding");
		exit(1);
	}
	
	// Create log file 
	int fd_log = open(logs, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (fd_log == -1){
		perror("Failed to create log file\n");	
	}

	struct sigaction psa;
	psa.sa_handler = SIGPOLLHandler;

	if (sigaction(SIGPOLL, &psa, 0) < 0){
		perror("sigaction() failed for SIGPOLL");
	}

	if (sigfillset(&psa.sa_mask) < 0){ 
		perror("sigfillset() failed");
	}
	// No flags
	psa.sa_flags = 0;

	// We must own the socket to receive the SIGIO message
	if (fcntl(cc.sd_to_rcv, F_SETOWN, getpid()) < 0){
		perror("Unable to set process owner to us");
	}
	// Arrange for nonblocking I/O and SIGIO delivery
	if (fcntl(cc.sd_to_rcv, F_SETFL, O_NONBLOCK | FASYNC) < 0){
		perror("Unable to put client sock into non-blocking/async mode");
	}
	
	// Send audio content to the client
	
	// Create a UDP socket for seding audio content to client
	if ((cc.sd_to_send = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("ERROR on creating socket");
		exit(1);
	}
		
	// zero out the structure
	memset((char *) &their_addr, 0, addr_len);

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(cli_port);
	their_addr.sin_addr.s_addr = cc.cli_ip;
	bzero(&(their_addr.sin_zero), 8); // zero the rest of the struct
	
	printf("path name is %s\n",pathname);
	// Open the filename to read
	int fd = open(pathname, O_RDONLY, 0666);
	if (fd == -1) {
		perror("ERROR on specified file");
		exit(1);
	}

	int bytes_read, bytes_write;
	int flags = 0; // for start time 
	total_bytes_sent = 0;
	// Loops to read data, each time read size bytes
	memset(buf, 0, size);
	while (1) {
		printf("before read\n");
		bytes_read = read(fd, buf, size);
		if (bytes_read < 0) {
			printf("ERROR on reading file\n");
			exit(1);
		}
		
		if (bytes_read == 0) {
			printf("End reading.\n");
			break;
		}
		
		// Get the start time when sending the first packet
		if (!flags){
			gettimeofday(&start, NULL);
			flags = 1;
		}

		printf("before send, bytes read %d\n", bytes_read);
		// Send back to the client
		bytes_write = sendto(cc.sd_to_send, buf, bytes_read, 0, (struct sockaddr *) &their_addr, addr_len);
		if (bytes_write < 0){
			perror("ERROR: cannot write");
		}
		printf("bytes write %d\n", bytes_write);
		// Get time stamp after sending
		gettimeofday(&end, NULL);
		elapsed = elapsed_time(start, end);
		char temp[100];
		memset(temp, 0, 100);
		sprintf(temp, "%f: %d\n", elapsed, cc.tau);
		strcat(log, temp);

		total_bytes_sent += bytes_write;
		printf("Sent %d bytes to client with tau =%d, total = %ld\n", bytes_read, cc.tau, total_bytes_sent);
		// Sleep between sucessive packets
		packet_sleep(cc.tau);
		printf("Done sleeping\n");
		memset(buf, 0, size);
	}
	printf("Total bytes sent = %ld\n", total_bytes_sent);
	// Write to log file
	write(fd_log, log, strlen(log));
	close(fd_log); 
	close(fd);
	close(cc.sd_to_send);
	//close(cc.sd_to_rcv);
}


