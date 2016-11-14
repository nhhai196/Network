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

#include <pthread.h>

#define BUFFSIZE 40000

typedef struct {
	int buf_size;
	char * au_buff;
	int cbl;
	
	// enforce mutual exculsion to shared data
	pthread_mutex_t mutex;
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

// Function declarations
void SIGPOLLHandler(int sig);
void SIGALRMHandler(int sig);
void send_feedback();
void initialize();

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
	buf_sz = atoi(argv[7]);
	target_buf = atoi(argv[8]);
	strcpy(filename, argv[10]);
	mu = (int) (1000000/gama); 
	
	printf("tcp port: %d", tcp_port);
	
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
	
	// TODO : sleep 2.5 seconds
	
	// Open file /dev/audio
	fp = open("/dev/audio", O_WRONLY);
	if (fp == -1){
		perror("ERROR on open dev audio");
		exit(1);
	}
	
	ualarm(mu, mu);
	signal(SIGALRM, SIGALRMHandler);
}

void SIGPOLLHandler(int sig){
	int numbytes;
	char buffer[payload_size];
	struct sockaddr_in serv_add;
	memset(buffer, 0, payload_size);
	memset((char *) &serv_add, 0, addr_len);
	
	// Get the audio packet
	numbytes = recvfrom(sd_to_rcv, buffer, payload_size, 0, (struct sockaddr*) &serv_add, &addr_len);
	pthread_mutex_lock(&shared.mutex);
	//write to buffer
	int i = 0;
	for (i = 0; i < strlen(buffer); i++){
		if (shared.cbl < shared.buf_size){
			shared.au_buff[shared.cbl++] = buffer[i];
		}
	}
	
	send_feedback();	
	pthread_mutex_unlock(&shared.mutex);
}

void send_feedback(){
	char msg[100];
	memset(msg, 0, 100);
	sprintf(msg, "Q %d %d %d", shared.cbl, target_buf, gama);
	if (sendto(sd_to_send, msg, strlen(msg), 0, (struct sockaddr *) &their_addr, addr_len) < 0){
		perror("ERROR on send feedback");
	}
}

void SIGALRMHandler(int sig){
	pthread_mutex_lock(&shared.mutex);
	// read and then remove from buffer
	int size = gama * payload_size;
	if (shared.cbl >= size){
		write(fp, shared.au_buff, size);
		shared.cbl -= size;
		strcpy(shared.au_buff, shared.au_buff+size);
	}
	else {
		write(fp, shared.au_buff, shared.cbl);
		shared.cbl = 0;
		memset(shared.au_buff, 0, shared.buf_size);
	}
	pthread_mutex_unlock(&shared.mutex);
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
}
	
	


