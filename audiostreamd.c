/* audiostreamd.c
* @author Hai Nguyen 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
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
int tau;

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

//Function declarations
void SIGPOLLHandler();
void read_feedback(char buffer[]);
void update_throughput();
void initialize();
void handle_single_client(char* pathname, int serv_port, int cli_port);


int main(int argc, char * argv[]){
	// Variable declrations and initializations
	int udp_sd, tcp_sd;
	int tcp_port, new_tcp_sd;
	int serv_udp_port;
	int cli_udp_port;
	struct sockaddr_in tserv_add, userv_add, tcli_add, ucli_add;
	char buffer[BUFFSIZE];
	
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
	cc.tau = atoi(argv[4]);
	cc.mode = atoi(argv[5]);
	
	
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
		if (read(new_tcp_sd, buffer, BUFFSIZE) < 0){
			perror("ERROR on read tcp request");
		}
		
		//char * port_num = (char *) malloc(10);
		char * pathname = (char*) malloc(100);
		cli_udp_port = atoi(strtok(buffer, " "));
		pathname = strtok(NULL, " ");
		
		//Check if the file requested exists
		if( access( pathname, F_OK ) == -1 ) {
			// file doesn't exists
			perror("File requested doesn't exist");
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
		
		
		// Create child process
		int pid = fork();

		if (pid <0) {
			printf("ERROR on fork\n");
			exit(1);
		}
	
		if (pid == 0) { // Child process
			//Global variables

			handle_single_client(pathname, serv_udp_port, cli_udp_port);
		}
		else { // Parent process
		close(new_tcp_sd);
		}
	}
	
}

/*
void update_ccvars (struct ccvars cc, int tau, int tbl, int cbl, int gamma){
	cc.tau = tau;
	cc.tbl = tbl;
	cc.cbl = cbl;
	cc.gamma = gamma;
	cc.a = 1;
} */

void SIGPOLLHandler(int sig){
	int numbytes;
	int size = cc.payload_size;
	char buffer[size];
	struct sockaddr_in serv_add;
	memset(buffer, 0, size);
	memset((char *) &serv_add, 0, addr_len);
	
	// Get the feedback packet
	numbytes = recvfrom(cc.sd_to_rcv, buffer, size, 0, (struct sockaddr*) &serv_add, &addr_len);
	
	read_feedback(buffer);
	update_throughput();
}

void update_throughput(){
	int a = 1;
	if (cc.cbl < cc.tbl){
		cc.tau = cc.tau + a;
	}
	
	if (cc.cbl > cc.tbl){
		cc.tau = cc.tau - a;
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

void handle_single_client(char* pathname, int serv_port, int cli_port){
	int size = cc.payload_size;
	struct sockaddr_in my_addr, their_addr;
	struct timeval start, end;
	char buf[size];
	
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
	
	// Open the filename to read
	int fd = open(pathname, O_RDONLY);
	if (fd == -1) {
		perror("ERROR on specified file");
		exit(1);
	}

	int bytes_read, bytes_write;
	// Loops to read data, each time read size bytes
	while (1) {
		bzero(buf, size);
		bytes_read = read(fd, buf, size);
		if (bytes_read < 0) {
			printf("ERROR on reading file\n");
			exit(1);
		}
		
		if (bytes_read == 0) {
			printf("End reading.\n");
			break;
		}
		
		// Get time stamp before sending
		gettimeofday(&start, NULL);
		
		// Send back to the client
		bytes_write = sendto(cc.sd_to_send, buf, bytes_read, 0, (struct sockaddr *) &their_addr, addr_len);
		if (bytes_write != bytes_read){
			perror("ERROR: cannot write bytes read");
		}
		
		// Sleep between sucessive packets
		usleep(cc.tau);
	}
}


