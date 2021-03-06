/* audiolisten.c
* @author Hai Nguyen 
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

#include <errno.h>
#include <pthread.h>

#define BUFFSIZE 40000

typedef struct {
	int buf_size;
	char * au_buff;
	int cbl;
	
	// enforce mutual exculsion to shared data
	pthread_mutex_t mutex;
	pthread_cond_t notFull; 
	//pthread_cond_t notEmpty;
} sbuf_t;

// Global variables
sbuf_t shared;
int sd_to_send;
int sd_to_rcv;
int payload_size; 
int packet_spacing;
int playback_del;
int gama;
int buf_sz;
int target_buf; 
int mu;
int serv_udp_port;
int cli_udp_port;
struct hostent * serv_ip; // server IP
struct sockaddr_in their_addr;
int addr_len = sizeof(their_addr);
int fp;
long total_bytes_rcv = 0;
long total_bytes_wrote =0;
struct timeval start, end; 
int flags = 0;
int fd_log;
char * logc ;
char * logfilec;

// Function declarations
void SIGPOLLHandler(int sig);
void SIGALRMHandler(int sig);
void send_feedback();
void initialize();
void prefetch();

int main(int argc, char * argv[]){
	// Variable declrations and initializations
	int tcp_sd, new_tcp_sd;
	int tcp_port;
	int n;
	char filename[100], buffer[BUFFSIZE];
	struct sockaddr_in my_addr;

	// Check whether the number of input arguments is correct
	if (argc != 11){
		printf("Usage: ./audiolisten server-ip server-tcp-port client-udp-port payload-size playback-del gama buf-sz target-buf logfile-c filename\n");
		exit(0);
	}
	
	// Convert Inputs
	if ((serv_ip = gethostbyname(argv[1])) == NULL) {
		printf("ERROR: the host name %s doesn't exist\n", argv[1]);
		exit(0);
	}
	
	tcp_port = atoi(argv[2]);
	cli_udp_port = atoi(argv[3]);
	payload_size = atoi(argv[4]);
	playback_del = atoi(argv[5]);
	gama = atoi(argv[6]);
	buf_sz = 1024 * atoi(argv[7]); // Bytes
	target_buf = 1024 * atoi(argv[8]); // Bytes
	logfilec = argv[9];
	strcpy(filename, argv[10]);
	mu = (int) (1000000/gama); // micro seconds
	
	printf("tcp port: %d\n", tcp_port);
	
	//Create a TCP socket 
	tcp_sd = socket(AF_INET, SOCK_STREAM, 0);

	if (tcp_sd < 0) {
		printf("ERROR opening socket\n");
		exit(1);
	}
	
	// Initialize socket struct
	bzero((char *) &their_addr, sizeof(their_addr));

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(tcp_port);
	their_addr.sin_addr = *((struct in_addr *)serv_ip->h_addr);
	bzero(&(their_addr.sin_zero), 8); // zero the rest of the struct

	if (connect(tcp_sd, (struct sockaddr *)&their_addr, sizeof(struct sockaddr)) == -1) {
		perror("ERROR on connecting TCP");
		exit(1);
	}
	
	// Create the request
	memset(buffer, 0, BUFFSIZE);
	sprintf(buffer, "%d %s", cli_udp_port, filename);
	
	// Send the request to the server
	if (( n= write(tcp_sd, buffer, strlen(buffer))) < 0){
		perror("ERROR on writing to TCP socket");
		exit(1);
	}
	
	// Receive the answer
	
	memset(buffer, 0, BUFFSIZE);
	if (n = read(tcp_sd, buffer, BUFFSIZE) < 0){
		perror("ERROR on receiving");
		exit(1);
	}

	if (buffer[0] == 'K' && buffer[1] == 'O'){
		perror("ERROR on filename");
		exit(1);
	}
	
	if (buffer[0] = 'O' && buffer[1] == 'K'){
		printf("Request accepted\n");
		serv_udp_port = atoi(buffer+2);
		
	}
	
	// Initialize variables
	initialize();
	
	// Create a UDP socket for receiving audio content
	if ((sd_to_rcv = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("ERROR on creating socket");
		exit(1);
	}

	// zero out the structure
	memset((char *) &my_addr, 0, addr_len);

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(cli_udp_port);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	bzero(&(my_addr.sin_zero), 8); // zero the rest of the struct

	// bind socket to port 
	if (bind(sd_to_rcv, (struct sockaddr *) &my_addr, addr_len) < 0){
		perror("ERROR on binding");
		exit(1);
	}

	// Create log file 
	fd_log = open(logfilec, O_WRONLY | O_CREAT | O_TRUNC, 0666);
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
	if (fcntl(sd_to_rcv, F_SETOWN, getpid()) < 0){
		perror("Unable to set process owner to us");
	}
	// Arrange for nonblocking I/O and SIGIO delivery
	if (fcntl(sd_to_rcv, F_SETFL, O_NONBLOCK | FASYNC) < 0){
		perror("Unable to put client sock into non-blocking/async mode");
	}
	
	// Create a UDP socket for sending feedback to server
	if ((sd_to_send = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("ERROR on creating socket");
		exit(1);
	}
	
	// Initialize socket struct
	bzero((char *) &their_addr, sizeof(their_addr));

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(serv_udp_port);
	their_addr.sin_addr = *((struct in_addr *)serv_ip->h_addr);
	bzero(&(their_addr.sin_zero), 8); // zero the rest of the struct
	
	// sleep 2.5 seconds for prefetch
	prefetch();
	printf(" CBL after prefetch %d\n", shared.cbl);
	
	// Open file /dev/audio
	fp = open("/dev/audio", O_RDWR, 0666);
	if (fp == -1){
		perror("ERROR on open dev audio");
		exit(1);
	}
	

	ualarm(mu, mu);
	signal(SIGALRM, SIGALRMHandler);
	while(1);
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

void SIGPOLLHandler(int sig){
	printf("SIGPOLL with sig = %d\n", sig);
	int numbytes, bytes_copied ;
	double elapsed;

	do{
		char buffer[payload_size];
		struct sockaddr_in serv_add;
		memset(buffer, 0, payload_size);
		memset((char *) &serv_add, 0, addr_len);	
		if (!flags){
			gettimeofday(&start, NULL);
			logc = (char*) malloc(10000000);
			flags = 1;		
		}
	
		// Get the audio packet
		numbytes = recvfrom(sd_to_rcv, buffer, payload_size, 0, (struct sockaddr*) &serv_add, &addr_len);
		
		// Get time stamp after receiving
		gettimeofday(&end, NULL);
		elapsed = elapsed_time(start, end);
		if (numbytes < 0){
			if (errno != EWOULDBLOCK){
				printf("ERROR %d on recvfrom\n", errno);
			}
		}

		if (numbytes > 0){
			bytes_copied = 0;
			printf("Received an audio packet of length %d\n", numbytes);
			pthread_mutex_lock(&shared.mutex);

			if (shared.cbl >= shared.buf_size - numbytes){
				pthread_cond_wait(&shared.notFull, &shared.mutex);
			}
			//write to buffer
			int i = 0;
			for (i = 0; i < numbytes; i++){
				if (shared.cbl < shared.buf_size){
					shared.au_buff[shared.cbl++] = buffer[i];
					bytes_copied++;
				}
			}

			char temp[100];
			memset(temp, 0, 100);
			sprintf(temp, "%f %d\n", elapsed, shared.cbl);
			strcat(logc, temp);

			printf("Copied %d Bytes to audio buffer\n", bytes_copied);
			send_feedback();	
			pthread_mutex_unlock(&shared.mutex);
			total_bytes_rcv += numbytes;

			if (numbytes < payload_size){
				printf("Exit from sigpoll\n");
				printf("Total bytes rcv %ld, total bytes wrote %ld\n", total_bytes_rcv, total_bytes_wrote);
				write(fd_log, logc, strlen(logc));
				pthread_mutex_destroy(&shared.mutex); 
				close(fd_log);
				exit(0);
			}
		}
	} while (numbytes>= 0);
}

void send_feedback(){
	char msg[100];
	memset(msg, 0, 100);
	sprintf(msg, "Q %d %d %d", shared.cbl, target_buf, gama);
	if (sendto(sd_to_send, msg, strlen(msg), 0, (struct sockaddr *) &their_addr, addr_len) < 0){
		perror("ERROR on send feedback");
	}
	printf("Sent a feedback packet to server with cbl = %d\n", shared.cbl);
}

void SIGALRMHandler(int sig){
	printf("SIGALRM with sig = %d\n", sig);
	int bytes_write;
	pthread_mutex_lock(&shared.mutex);
	// read and then remove from buffer
	int size = payload_size;
	
	if (shared.cbl > 0){
		if (shared.cbl >= size){
			bytes_write = write(fp, shared.au_buff, size);
			if (bytes_write != size){
				printf("ERROR on write to /dev/audio\n");
			}
			shared.cbl -= bytes_write;
			strcpy(shared.au_buff, shared.au_buff+bytes_write);
			total_bytes_wrote += bytes_write;
		}
		else {
			bytes_write = write(fp, shared.au_buff, shared.cbl);
			//if (bytes_write != shared.cbl){
			//	printf("ERROR on write to /dev/audio\n");
			//}
			total_bytes_wrote += bytes_write;
			shared.cbl = 0;
			memset(shared.au_buff, 0, shared.buf_size);
		}
		printf("Wrote %d Bytes to /dev/audio\n", bytes_write);
		//send_feedback();
		if (shared.cbl < shared.buf_size - payload_size){
			pthread_cond_signal(&shared.notFull);
		}
	}
	pthread_mutex_unlock(&shared.mutex);
	signal(SIGALRM, SIGALRMHandler);
}

/*
void tokens(char a[], char first[], char last[]){
	char * temp;
	temp = strtok(a, " ");
	strcpy(first, temp);
	temp = strtok(NULL, " ");
	strcpy(last, temp);
} */

void initialize(){
	shared.buf_size = buf_sz;
	shared.au_buff = (char *) malloc(buf_sz);
	memset(shared.au_buff, 0, shared.buf_size);
	shared.cbl = 0;
	pthread_mutex_init(&shared.mutex, NULL); 
	pthread_cond_init(&shared.notFull, NULL);
	//pthread_cond_init(&shared.notEmpty, NULL);
}
	
void prefetch(){
	struct timespec start, remain;
	start.tv_sec = 2; 
	start.tv_nsec = 5 * 10^8; 
	while (nanosleep(&start,&remain) == -1){	
		start = remain;
		remain.tv_sec = 0;
		remain.tv_nsec = 0;
	}  		
}

