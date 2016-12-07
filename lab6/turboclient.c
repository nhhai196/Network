/* fileclient.c
* @author Hai Nguyen
*/

// Client
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
#include <sys/time.h>

//Protocol specific socket:

#include <netinet/tcp.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

#define BUFSIZE 256
#define COMMAND "date"

//Variables for window sliding (reliable)
int RWS = 32; // receiver window size
int LFR = 0; // last frame received 
int LAF = 32; // largest acceptable frame 
char * rcv_buf[32]; // An array for storing pending ACK packet

// Function to compute the elasped time between start and end
double elapsed_time(struct timeval start, struct timeval end){
	double result, sec, usec;
	sec = (end.tv_sec - start.tv_sec);	
	usec = (end.tv_usec - start.tv_usec);
	//printf("sec is %lld, usec is %lld\n", sec, usec);
	result =sec * 1000 + usec/1000; 
		 
	return result;
}

/* Main function */
int main(int argc, char *argv[]) {
	// Variable declrations and initializations
	int sd, fd, port_num, blocksize;
	int bytes_read, bytes_write;
	double throughput, elapsedtime;
	unsigned long numloops = 0; 
	unsigned long total_bytes = 0;
	char request[BUFSIZE];
	struct sockaddr_in serv_add;
	struct hostent * server;
	struct timeval start, end;
	socklen_t len = sizeof(serv_add);
	
	// Check if the number of arguments input is correct
	if (argc != 6) {
		printf("Usage: fileclient hostname portnumber secretkey filename configfile.dat\n");
		exit(1);
	}

	// Convert portnumber from an input string to an integer 
	port_num = atoi(argv[2]);

	// Create a UDP socket
	if ((sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		perror("ERROR on creating socket");
		exit(1);
	}
	
	server = gethostbyname(argv[1]);

	if (server == NULL) {
		printf("ERROR: the host name %s doesn't exist\n", argv[1]);
		exit(0);
	}

	// Zero out address
	bzero((char *) &serv_add, sizeof(serv_add));
	// Initialize struct
	serv_add.sin_family = AF_INET;
	serv_add.sin_port = htons(port_num);
	bcopy((char *)server->h_addr, (char *)&serv_add.sin_addr.s_addr, server->h_length);

	// Create and store the request
	bzero(request, BUFSIZE);
	strcat(request, "$");
	strcat(request, argv[3]);
	strcat(request, "$");
	strcat(request, argv[4]);

	// Sent the request to the server
	if (sendto(sd, request, strlen(request), 0, (struct sockaddr *) &serv_add, len) < 0){
		perror("ERROR on send to\n");
		exit(1);
	}
	printf("A request has sent to the server: %s\n", request);

	// Get blocksize from the input file configfile.dat
	FILE *myfile = fopen(argv[5], "r");
	fscanf(myfile, "%d", &blocksize);
	printf("Block size: %d\n", blocksize);

	// A buffer for storing data when reading from the server
	char buf[blocksize];
	bzero(buf, blocksize);

	// Read server response
	while (1) { // Loops to read data, each time read blocksize bytes
		memset((char*) &serv_add, 0, len);
		bytes_read = recvfrom(sd, buf, blocksize, 0, (struct sockaddr * ) &serv_add, &len);
		if (bytes_read < 0) {
			printf("ERROR on reading the socket\n");
			exit(1);
		}
		if (bytes_read == 0) {
			if (numloops != 0){ // End of reading data
				printf("Ends reading data from the server\n");
			// get time of day after the last read() call
			gettimeofday(&end, NULL);

			// Print out the number of bytes read and the completion time
			printf("The total number of bytes read: %ld\n", total_bytes);
			elapsedtime = elapsed_time(start, end);
			printf("The elapsed time is %.3f ms\n", elapsedtime);

			// Compute the throughput and print it out 
			throughput = total_bytes/elapsedtime;
			printf("Reliable throughput: %.3f KB\n", throughput);
			}
			else {
				printf("No response!\n");
			}

			break;
		}

		// Increase the number of times read data
		numloops++;
		
		// Get time of day after the first read
		if (numloops == 1){
			gettimeofday(&start, NULL);

			//Check if the filename already exists
			if (access (argv[4], F_OK) != -1) { 
				printf("ERROR: File already exists\n");
				exit(1);
			}
		
			// Otherwise open it to save the downloaded data
			fd = open(argv[4], O_CREAT | O_RDWR);
			if (fd == -1){
				perror("ERROR on opening file");
				exit(1);
			}
		}
		
		// get the sequence number and actual data;
		int seq_num;
		char * temp;
		char data[blocksize];
		temp = strtok(buf, "$");
		seq_num = atoi(temp);
		temp = strtok(NULL, "$");
		strcpy(data, temp);
		printf("seq_num = %d\n", seq_num);
		if (seq_num <= LFR || seq_num > LAF){
			printf("The frame is discarded since LFR = %d, LAF = %d\n", LFR, LAF);
		}
		else { // accept the frame
			// Save it to the buffer
			rcv_buf[seq_num - LFR-1] = data;
			int num = 0;
			
			// Save old LFR before updating
			int old_LFR = LFR;
			//Update LFR
			while (rcv_buf[num] != NULL & num < (LAF - LFR)){
				num++;
			}
			LFR += num - 1;
			if (old_LFR == LFR && LFR != 0){
				//Send NAK
				char array[BUFSIZE];
				memset(array, 0, BUFSIZE);
				sprintf(array,"NAK%d", LFR + 1);
				if(sendto(sd, array, strlen(array), 0, (struct sockaddr *) &serv_add, len) < 0){
					perror("ERROR on sendto");
					exit(1);
				}
			}
			else{
				//Send cumulative ACK
				char array[BUFSIZE];
				memset(array, 0, BUFSIZE);
				sprintf(array,"ACK%d", LFR + 1);
				if(sendto(sd, array, strlen(array), 0, (struct sockaddr *) &serv_add, len) < 0){
					perror("ERROR on sendto");
					exit(1);
				}
				printf("Sent cumulative ACK to server\n");
				//Flush buffer upto LFR to file
				int i;
				for (i = 0; i < LFR - old_LFR; i++){
					bytes_write = write(fd, rcv_buf[i], 990);
					total_bytes += bytes_write;
					rcv_buf[i] = NULL;
				}
				// Update LAF 
				LAF = RWS + LFR;
			}
		}
		

		// Add to the total_bytes read varibale
		total_bytes += bytes_read;

		bzero(buf, blocksize);
	}

	// Close file descriptor
	close(fd);

	return 0;
}



