/* overlayrouter.c
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
#include <time.h>
#include <sys/time.h>

#define BUFSIZE 1500
#define PORT 20000

typedef struct {
	unsigned long src_IP;
	unsigned short int src_port;
} Label;

typedef struct {
	Label src;
	Label dst;
	int flags; // for temporary entry 
} Entry;

// Routing table
Entry * route_table[100];
int pos =0;

int token_payload(char * buffer, char * tokens[]){
	int i = 0;
	char * temp; 
	temp = strtok(buffer, "$");
	while (temp != NULL){
		tokens[i++] = temp;
		//printf("%s\n", temp);
		temp = strtok(NULL, "$");
	}
	
	return i--;
}

void handle_alarm( int sig ) {
	signal(sig, SIG_IGN);
	printf("No confirmation after 30 seconds, discard the temporary routing table entry\n");
	exit(1);
}

// function to check whether the last IP address inscribed in the message
// matches its own IP-address 
int matchedIP(char * ptr){
	char * hostname;
	char * ip;
	hostname = (char *) malloc(50);
	ip = (char *) malloc(10);
	struct hostent * he;
	struct in_addr **addr_list;
	int i;
	
	gethostname(hostname, 100* sizeof(hostname));
	printf("Host name is %s\n", hostname);
	
	if ((he = gethostbyname(hostname)) == NULL){
		printf("Error on gethostbyname\n");
		return 0;
	}
	
	addr_list = (struct in_addr **) he->h_addr_list;
	for(i = 0; addr_list[i] != NULL; i++) {
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr_list[i]) );
		break;
	}
	
	//printf("My own IP address is %s.\n", ip);
	
	if (strcmp(ip, ptr) == 0){
		printf("The IP addresseses are matched.\n");
		return 1;
	}
	
	printf("The IP addresses are mismatched.\n");
	
	return 0;
}

int main(int argc, char * argv[]){
	// Variable declrations and initializations
	struct sockaddr_in serv_add, cli_add, rserv_add, newserv_add;
	int sd, n, l, newsd, sockd, data_port;
	socklen_t len;
	char buffer[BUFSIZE], buf[BUFSIZE];
	char *serverIP = (char *) malloc(20);
	char *serverPort = (char *) malloc(20);
	struct hostent *he;
	struct sockaddr_in saved1, saved2, dst_add, src_add;
	char * tokens[20];
	int count;
	struct timeval start;
	
	// get the size of struct addr
	len = sizeof(cli_add);
	// Set the seed for the rand() function
	srand(time(NULL));

	// Check whether the number of input arguments is correct
	if (argc != 2){
		printf("Usage: ./overlayrouter server-port\n");
		exit(0);
	}

	// Create a UDP socket
	if ((sd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
		perror("ERROR on creating socket");
		exit(1);
	}

	// zero out the structure
	memset((char *) &serv_add, 0, sizeof(serv_add));

	serv_add.sin_family = AF_INET;
	serv_add.sin_port = htons(atoi(argv[1]));
	serv_add.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind socket to port 
	if (bind(sd, (struct sockaddr *) &serv_add, sizeof(serv_add)) < 0){
		perror("ERROR on binding");
		exit(1);
	}
	
	printf("Waiting for data...\n");
	// Keep listening for data
	while(1) {
		fflush(stdout);
		memset(buffer, 0, BUFSIZE);

		if ((n = recvfrom(sd, buffer, BUFSIZE, 0, (struct sockaddr * ) &cli_add, &len)) == -1){
			perror("ERROR on recvfrom");
			exit(0);
		}
		
		if(buffer[1] == '$'){// Reveived confirmation
			printf("Received confirmation %s from %s on port %d\n", 
			buffer, inet_ntoa(cli_add.sin_addr), ntohs(cli_add.sin_port));
			
			// Make routing table entry permanent
			
			// Perform route table lookup
			alarm(0); // cancel the alarm
			printf("Routing table entry is confirmed.\n");
			// Send a packet to previous hop to signify that the routing table entry is confirmed
			char payload[BUFSIZE];
			memset(payload, 0, BUFSIZE);
			sprintf(payload, "$$%s$%d$", tokens[count-1], data_port);
			// zero out the structure
			memset((char *) &serv_add, 0, sizeof(serv_add));
			
			serv_add.sin_family = AF_INET;
			serv_add.sin_port = htons(atoi(argv[1]));
			serv_add.sin_addr.s_addr = src_add.sin_addr.s_addr;
			
			if (sendto(sd, payload, strlen(payload), 0, (struct sockaddr *) &serv_add, len)< 0){
				perror("ERROR on first sendto");
				exit(1);
			}
			printf("Sent confirmation to previous router %s\n", inet_ntoa(src_add.sin_addr));
		}
		else {
			printf("Received a request from %s on port %d\n", 
			inet_ntoa(cli_add.sin_addr), ntohs(cli_add.sin_port));
			printf("payload: %s\n", buffer);
			src_add = cli_add;
			// Set alarm for 30 seconds
			signal(SIGALRM, handle_alarm);
			alarm(30);
/*		int pid = fork();*/
/*		if (pid < 0){*/
/*			perror("ERROR on fork");*/
/*		}*/
/*		else if (pid == 0){ // child process*/
			char stripped_buffer[BUFSIZE];
			// strip its own IP address
			buffer[strlen(buffer)-1] = '\0';
			strcpy(stripped_buffer, buffer);
			char *temp; 
			temp = strrchr(stripped_buffer, '$');
			temp++;
			*temp = '\0';
			//printf("Stripped buffer is %s\n", stripped_buffer);
			
			// Tokenize the payload 
			count = token_payload(buffer, tokens);
			//printf("Number of tokens is %d, last token: %s.\n", count, tokens[count-1]);
			
			// Check if not matched, then discard 
			if (!matchedIP(tokens[count-1])){
				printf("Discard the path-setup message\n");
				continue; 
			}

			data_port = 10000 + rand()%50000;
			memset(buf, 0, BUFSIZE);
			sprintf(buf, "%d", data_port);
			
			// Create a new server UDP socket on the new data-port-number for listening 
			if ((newsd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
				perror("ERROR on creating socket");
				exit(1);
			}

			// zero out the structure
			memset((char *) &newserv_add, 0, sizeof(newserv_add));

			newserv_add.sin_family = AF_INET;
			newserv_add.sin_port = htons(data_port);
			newserv_add.sin_addr.s_addr = htonl(INADDR_ANY);

			// bind socket to the new port 
			if (bind(newsd, (struct sockaddr *) &newserv_add, sizeof(newserv_add)) < 0){
				perror("ERROR on binding");
				exit(1);
			}

			// Send a UDP packet, containing data-port-number, back to overlaybuild/previous router
			if (sendto(newsd, buf, strlen(buf), 0, (struct sockaddr *) &cli_add, len)< 0){
				perror("ERROR on first sendto");
				exit(1);
			}
			printf("Sent data port number %d to the previous router\n", data_port);
			
			// Send a UDP packet containing stripped payload to the next router if not the last router
			if (count != 3){
				printf("This is not the last router\n");
				
				// get the IP address of the next router
				
				// zero out the structure
				memset((char *) &cli_add, 0, sizeof(cli_add));

				if ((he=gethostbyname(tokens[count-2])) == NULL) { // get the next router info
					perror("gethostbyname");
					exit(1);
				}

				cli_add.sin_family = AF_INET;
				cli_add.sin_port = htons(atoi(argv[1]));
				cli_add.sin_addr = *((struct in_addr *)he->h_addr);
				bzero(&(cli_add.sin_zero), 8); // zero the rest of the struct
				
				if (sendto(newsd, stripped_buffer, strlen(stripped_buffer), 0, (struct sockaddr *) &cli_add, len)< 0){
					perror("ERROR on first sendto");
					exit(1);
				}
				
				printf("Sent stripped payload = %s to the next router\n", stripped_buffer); 
				char new_buffer[BUFSIZE];
				memset(new_buffer,0, BUFSIZE);
				memset((char *) &cli_add, 0, sizeof(cli_add));
				
				if ((n = recvfrom(newsd, new_buffer, BUFSIZE, 0, (struct sockaddr * ) &cli_add, &len)) == -1){
					perror("ERROR on recvfrom");
					exit(0);
				}
				printf("Received a message %s from %s\n", buffer, inet_ntoa(cli_add.sin_addr));
				dst_add = cli_add;
				printf("Updating routing table at router %s\n", tokens[count-1]);
				gettimeofday(&start, NULL);
				printf("At time stamp: %ld(s)%ld(us),\n", start.tv_sec, start.tv_usec);
				printf("the two labels below are added to route table\n");
				printf("(%s, %d)", inet_ntoa(src_add.sin_addr), ntohs(src_add.sin_port));
				printf(" and (%s,%d)\n", inet_ntoa(dst_add.sin_addr), ntohs(dst_add.sin_port));
				
				continue;
			}
			else { // the last router
				// send a UDP packet with payload $$routerk-IP$data-port-k$,
				// which signifies to the previous hop router(k-1)-IP that
				// the routing table entry is confirmed
				printf("This is the last router\n");
				
				// Update 
				printf("Dst IP: %s, dst port: %s\n", tokens[0], tokens[1]);
				if ((he=gethostbyname(tokens[0])) == NULL) { // get the next router info
					perror("gethostbyname");
					exit(1);
				}
				
				dst_add.sin_port = htons(atoi(tokens[1]));
				dst_add.sin_addr = *((struct in_addr *)he->h_addr);
				printf("Updating routing table at router %s\n", tokens[count-1]);
				gettimeofday(&start, NULL);
				printf("At time stamp: %ld(s)%ld(us),\n", start.tv_sec, start.tv_usec);
				printf("the two labels below are added to route table\n");
				printf("(%s, %d)", inet_ntoa(src_add.sin_addr), ntohs(src_add.sin_port));
				printf(" and (%s,%d)\n", inet_ntoa(dst_add.sin_addr), ntohs(dst_add.sin_port));
				
				// Send a packet to previous hop to signify that the routing table entry is confirmed
				char payload[BUFSIZE];
				memset(payload, 0, BUFSIZE);
				sprintf(payload, "$$%s$%d$", tokens[count-1], data_port);
				// zero out the structure
				memset((char *) &serv_add, 0, sizeof(serv_add));
			
				serv_add.sin_family = AF_INET;
				serv_add.sin_port = htons(atoi(argv[1]));
				serv_add.sin_addr.s_addr = src_add.sin_addr.s_addr;
			
				if (sendto(sd, payload, strlen(payload), 0, (struct sockaddr *) &serv_add, len)< 0){
					perror("ERROR on first sendto");
					exit(1);
				}
				printf("Sent confirmation to previous router %s\n", inet_ntoa(src_add.sin_addr));
			}
		}
			
			printf("Server on new port %d...\n", data_port);
			while(1){
				memset(buffer,0, BUFSIZE);
				memset((char *) &cli_add, 0, sizeof(cli_add));
				
				if ((n = recvfrom(newsd, buffer, BUFSIZE, 0, (struct sockaddr * ) &cli_add, &len)) == -1){
					perror("ERROR on recvfrom");
					exit(0);
				}
				
				//printf("Received packet from %s on port %d\n", inet_ntoa(cli_add.sin_addr), ntohs(cli_add.sin_port));
				//printf("Src :  %s on port %d\n", inet_ntoa(src_add.sin_addr), ntohs(src_add.sin_port));
				//printf("Dst : %s on port %d\n", inet_ntoa(dst_add.sin_addr), ntohs(dst_add.sin_port));
				if (cli_add.sin_addr.s_addr == dst_add.sin_addr.s_addr){
					//printf("Send backward\n");
					if (sendto(newsd, buffer, strlen(buffer), 0, (struct sockaddr *) &src_add, len) < 0){
						perror("ERROR on sendto\n");
						exit(1);
					}
				}
				else {
					//printf("Send forward\n");
					if (sendto(newsd, buffer, strlen(buffer), 0, (struct sockaddr *) &dst_add, len) < 0){
						perror("ERROR on sendto\n");
						exit(1);
					}					
				}
				
				if (strcmp (buffer, "terve") == 0 || n == 3){
					printf("********************************************************\n");
					break;
				}
			}
			
/*		else { // Parent process*/
/*			close(newsd);*/
/*			//close(sockd);*/
/*		}*/
	}

	// Close socket descriptor
	close(sd);
	return 0;
}
