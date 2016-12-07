/* fileserver.c
* @author Hai Nguyen
*/

// tcp server

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

#define BUFSIZE 1000

//Global variables
int blocksize;
int len = sizeof (struct sockaddr_in);

//Variables for window sliding (reliable)
unsigned int seq_num = 1;
int SWS = 32; // sender window size
int LAR = 0; // last acknowledgement received 
int LFS = 0; // last frame sent 
char * snd_buf[32]; // An array for storing pending ACK packet

int sockd;
struct sockaddr_in cli_add;
int fd; 

void handle_single_client(char * key, char * arg3);
void SIGPOLLHandler(int sig); 
int lenHelper(unsigned int x) ;

int main(int argc, char * argv[]) {
	// Variable declrations and initializations
	int sd, newsd;
	int slen, n;
	int port_num;
	struct sockaddr_in serv_add;
	int pid;
	char buffer[BUFSIZE];

	// Check if the number of arguments is correct
	if (argc != 5) {
		printf("Usage: fileserver portnumber secretkey configfile.dat lossnum\n");
		exit(0);
	}

	// Check length of secret key
	slen = strlen(argv[2]);
	if (slen <10 || len > 20) {
		printf("ERROR secretkey too short or too long\n");
		exit(1);
	}

	// Create a UDP socket
	if ((sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		perror("ERROR on creating socket");
		exit(1);
	}
	
	// Initilaize socket struct
	port_num = atoi(argv[1]);
	bzero((char *) &serv_add, sizeof(serv_add));

	serv_add.sin_family = AF_INET;
	serv_add.sin_port = htons(port_num);
	serv_add.sin_addr.s_addr = INADDR_ANY;

	// Bind the host address using bind() 
	if (bind(sd, (struct sockaddr *) &serv_add, sizeof(serv_add)) < 0){
		printf("ERROR on binding\n");
		exit(1);
	}

	len = sizeof(cli_add);
	while(1) {
		//printf("Waiting for data...");
		fflush(stdout);
		memset(buffer, 0, BUFSIZE);
		//bzero((char *) &cli_add, len);
		
		if ((n = recvfrom(sd, buffer, BUFSIZE, 0, (struct sockaddr * ) &cli_add, &len)) == -1){
			perror("ERROR on recvfrom");
			exit(0);
		}
		
		printf("Received request from %s\n", inet_ntoa(cli_add.sin_addr));
		printf("Request: %s\n", buffer);

		// Create child process
		pid = fork();

		if (pid <0) {
			printf("ERROR on fork\n");
			exit(1);
		}
		 
		if (pid == 0) { // Child process 
			//handle_single_client(argv[2], argv[3]);
			printf("Start handle client request\n");
			int bytes_rcv, bytes_snd;
			char * filename;
			char * secretkey;

			struct sockaddr_in newserv_add;

			// Get the secretkey and file name from the request
			secretkey = strtok(buffer, "$");
			filename = strtok(NULL, "$");
			printf("file name: %s\n", filename);
			// Check secretkey
			if (strcmp(secretkey, argv[2]) != 0){
				printf("ERROR: wrong key\n");
				exit(1);
			}
			else { // Correct
				printf("Key is correct\n");
				// Get the block size saved in the given file
				FILE *myfile = fopen(argv[3], "r");
				fscanf(myfile, "%d", &blocksize);
				printf("Block size: %d\n", blocksize);

				// A buffer to read data
				char buf[blocksize];
				bzero(buf, blocksize);

				// Get the name of the directory in which we read data
				char file_path[100];
				bzero(file_path, 100);
				sprintf(file_path, "temp/%s", filename);

				// Open the filename to read
				fd = open(file_path, O_RDONLY);
				if (fd == -1) {
					perror("ERROR on specified file");
					exit(1);
				}
				int data_port = 10000 + rand()%50000;
				memset(buffer, 0, BUFSIZE);
				sprintf(buffer, "filedeposit/%d", data_port);
	
				// Create a new server UDP socket on the new data-port-number for listening 
				if ((sockd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
					perror("ERROR on creating socket");
					exit(1);
				}

				// zero out the structure
				memset((char *) &newserv_add, 0, sizeof(newserv_add));

				newserv_add.sin_family = AF_INET;
				newserv_add.sin_port = htons(data_port);
				newserv_add.sin_addr.s_addr = htonl(INADDR_ANY);

				// bind socket to the new port 
				if (bind(sockd, (struct sockaddr *) &newserv_add, sizeof(newserv_add)) < 0){
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
				if (fcntl(sockd, F_SETOWN, getpid()) < 0){
					perror("Unable to set process owner to us");
				}
				// Arrange for nonblocking I/O and SIGIO delivery
				if (fcntl(sockd, F_SETFL, O_NONBLOCK | FASYNC) < 0){
					perror("Unable to put client sock into non-blocking/async mode");
				}
		
				// Loops to read data, each time read blocksize bytes
				//while (1) {
					int size = blocksize - lenHelper(seq_num) - 2;
					bytes_rcv = read(fd, buf, size);
					if (bytes_rcv < 0) {
						printf("ERROR on reading file\n");
						exit(1);
					}
					if (bytes_rcv == 0) {
						printf("End reading.\n");
						break;
					}
		
					// Add seq_num to payload
					char payload[blocksize];
					memset(payload, 0, blocksize);
					sprintf(payload, "$%d$%s", seq_num, buf);
		
					// Send back to the client
					if(sendto(sockd, payload, strlen(payload), 0, (struct sockaddr *) &cli_add, len) < 0){
						perror("ERROR on sendto");
						exit(1);
					}
					printf(" A packet with seq_num = %d sent\n", seq_num);
					// Update variables
					LFS = seq_num;
					seq_num++;
		
					bzero(buf, blocksize);
				//}
			}
			// Close file descriptor
			//close(fd);
			 
			//exit(0);
		}
		else { // Parent process
			//close(newsd);
			//printf("***************************************\n");
		}
	}

	close(sd);
	return 0;
}

int lenHelper(unsigned int x) {
    if(x>=1000000000) return 10;
    if(x>=100000000) return 9;
    if(x>=10000000) return 8;
    if(x>=1000000) return 7;
    if(x>=100000) return 6;
    if(x>=10000) return 5;
    if(x>=1000) return 4;
    if(x>=100) return 3;
    if(x>=10) return 2;
    return 1;
}


/* Function for handling a single client request */
void handle_single_client(char * key, char * arg3){
	printf("Start handle client request\n");
	int n, fd;
	int bytes_rcv, bytes_snd;
	char * filename;
	char * secretkey;
	char buffer[BUFSIZE];
	bzero(buffer, BUFSIZE);
	struct sockaddr_in newserv_add;
	printf("Check point");
	// Get the secretkey and file name from the request
	secretkey = strtok(buffer, "$");
	filename = strtok(NULL, "$");

	// Check secretkey
	if (strcmp(secretkey, key) != 0){
		printf("ERROR: wrong key\n");
		exit(1);
	}
	else { // Correct
		printf("Key is correct\n");
		// Get the block size saved in the given file
		FILE *myfile = fopen(arg3, "r");
		fscanf(myfile, "%d", &blocksize);
		printf("Block size: %d\n", blocksize);

		// A buffer to read data
		char buf[blocksize];
		bzero(buf, blocksize);

		// Get the name of the directory in which we read data
		char file_path[100];
		bzero(file_path, 100);
		sprintf(file_path, "filedeposit/%s", filename);

		// Open the filename to read
		fd = open(file_path, O_RDONLY);
		if (fd == -1) {
			perror("ERROR on specified file");
			exit(1);
		}
		int data_port = 10000 + rand()%50000;
		memset(buffer, 0, BUFSIZE);
		sprintf(buffer, "%d", data_port);
	
		// Create a new server UDP socket on the new data-port-number for listening 
		if ((sockd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
			perror("ERROR on creating socket");
			exit(1);
		}

		// zero out the structure
		memset((char *) &newserv_add, 0, sizeof(newserv_add));

		newserv_add.sin_family = AF_INET;
		newserv_add.sin_port = htons(data_port);
		newserv_add.sin_addr.s_addr = htonl(INADDR_ANY);

		// bind socket to the new port 
		if (bind(sockd, (struct sockaddr *) &newserv_add, sizeof(newserv_add)) < 0){
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
		if (fcntl(sockd, F_SETOWN, getpid()) < 0){
			perror("Unable to set process owner to us");
		}
		// Arrange for nonblocking I/O and SIGIO delivery
		if (fcntl(sockd, F_SETFL, O_NONBLOCK | FASYNC) < 0){
			perror("Unable to put client sock into non-blocking/async mode");
		}
		
		// Loops to read data, each time read blocksize bytes
		while (1) {
			int size = blocksize - lenHelper(seq_num) - 2;
			bytes_rcv = read(fd, buf, size);
			if (bytes_rcv < 0) {
				printf("ERROR on reading file\n");
				exit(1);
			}
			if (bytes_rcv == 0) {
				printf("End reading.\n");
				break;
			}
		
			// Add seq_num to payload
			char payload[blocksize];
			memset(payload, 0, blocksize);
			sprintf(payload, "$%d$%s", seq_num, buf);
		
			// Send back to the client
			if(sendto(sockd, payload, strlen(payload), 0, (struct sockaddr *) &cli_add, len) < 0){
				perror("ERROR on sendto");
				exit(1);
			}
		
			// Update variables
			LFS = seq_num;
			seq_num++;
		
			bzero(buf, blocksize);
		}
	}
	// Close file descriptor
	close(fd);
	 
	exit(0);
}

void retransmit(int num){
	if (num - LAR < 1){
		printf("Something went wrong with NAK\n");
	}
	char *ptr = snd_buf[num-LAR-1];
	
	if(sendto(sockd, ptr, strlen(ptr), 0, (struct sockaddr *) &cli_add, len) < 0){
		perror("ERROR on sendto");
		exit(1);
	}
	printf("A packet with seq_num %d retransmitted\n", seq_num);
}

void send_packet(){
	int bytes_rcv;
	char buf[blocksize];
	bzero(buf, blocksize);
	int size = blocksize - lenHelper(seq_num) - 2;
	bytes_rcv = read(fd, buf, size);
	if (bytes_rcv < 0) {
		printf("ERROR on reading file\n");
		exit(1);
	}
	if (bytes_rcv == 0) {
		printf("End reading.\n");
		exit(0);
	}
	// Add seq_num to payload
	char payload[blocksize];
	memset(payload, 0, blocksize);
	sprintf(payload, "$%d$%s", seq_num, buf);
	
	// Send back to the client
	if(sendto(sockd, payload, strlen(payload), 0, (struct sockaddr *) &cli_add, len) < 0){
		perror("ERROR on sendto");
		exit(1);
	}
	printf("A packet with seq_num %d sent\n", seq_num);
	
	// Store the payload into sender buffer
	snd_buf[LFS-LAR] = payload;
	
	// Update variables
	LFS = seq_num;
	seq_num++;
}

void SIGALRMHandler(int sig){
	printf("SIGALRM with sig = %d\n", sig);
	// retransmit the packet
	//retransmit();
	signal(SIGALRM, SIGALRMHandler);
}

void handle_ACK(int x){
	if (x <= LAR){
		printf("Something Wrong\n");
		exit(1);
	}
	// FLush buffer up to x
	int i;
	for (i = 0; i < LFS -(x-LAR); i++){
		snd_buf[i] = snd_buf[i+x-LAR];
	}
	// Forwind LAR to x
	LAR = x;
	// Send upto SWS - (LFS - LAR)
	while ((SWS - LFS + LAR) > 0){
		send_packet();
		// Update LFS
		LFS++;
	}
}

void SIGPOLLHandler(int sig){
	printf("SIGPOLL with sig = %d\n", sig);
	int numbytes;
	//double elapsed;

	do{
		char buffer[blocksize];
		struct sockaddr_in their_addr;
		memset(buffer, 0, blocksize);
		memset((char *) &their_addr, 0, len);	

		// Get packet
		numbytes = recvfrom(sockd, buffer, blocksize, 0, (struct sockaddr*) &their_addr, &len);
		
		if (numbytes < 0){
			if (errno != EWOULDBLOCK){
				printf("ERROR %d on recvfrom\n", errno);
			}
		}

		if (numbytes > 0){
			printf("Received a packet of length %d\n", numbytes);
			
			if (strncmp(buffer, "ACK", 3) == 0){
				printf("Received an ACK with sequence number %s\n", buffer+3);
				int temp_seq = atoi(buffer+3);
				handle_ACK(temp_seq);
			}
			else if (strncmp(buffer, "NAK", 3) == 0){
				printf("Received a NAK with sequence number %s\n", buffer+3);
				int temp_seq = atoi(buffer+3);
				retransmit(temp_seq);
			}
			//total_bytes_rcv += numbytes;
		}
	} while (numbytes>= 0);
}

