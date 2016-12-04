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

#define BUFSIZE 1500
#define PORT 20000

struct {
	unsigned long src_IP;
	unsigned short int src_port; 
	unsigned long dst_IP;
	unsigned short int dst_port;
	int flags; // for temporary entry 
} route_entry;

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
	
	printf("My own IP address is %s\n", ip);
	
	if (strcmp(ip, ptr) == 0){
		printf("Matched\n");
		return 1;
	}
	
	printf("Mismatched\n");
	
	return 0;
}

int main(int argc, char * argv[]){
	// Variable declrations and initializations
	struct sockaddr_in serv_add, cli_add, rserv_add, newserv_add;
	int sd, n, l, newsd, sockd, newport;
	socklen_t len;
	char buffer[BUFSIZE], buf[BUFSIZE];
	char *serverIP = (char *) malloc(20);
	char *serverPort = (char *) malloc(20);
	int count = 0;
	struct hostent *he;
	struct sockaddr_in saved1, saved2;
	
	// get the size of struct addr
	len = sizeof(cli_add);

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
		
		printf("Received request %s from %s\n", buffer, inet_ntoa(cli_add.sin_addr));

		int pid = fork();
		if (pid < 0){
			perror("ERROR on fork");
		}
		else if (pid == 0){ // child process
			char stripped_buffer[BUFSIZE];
			// strip its own IP address
			strcpy(stripped_buffer, buffer);
			char *temp; 
			temp = strrchr(stripped_buffer, '$');
			*temp = '\0';
			printf("Stripped buffer is %s\n", stripped_buffer);
			
			// Tokenize the payload 
			char * tokens[20];
			int count; 
			count = token_payload(buffer, tokens);
			printf("Number of tokens is %d\n", count);
			
			// Check if not matched, then discard 
			if (!matchedIP(tokens[count-1])){
				//continue; 
			}

			srand(time(NULL));
			newport = 10000 + rand()%90000;
			memset(buf, 0, BUFSIZE);
			sprintf(buf, "%d", newport);

			// Send a UDP packet, containing data-port-number, back to overlaybuild/previous router
			if (sendto(sd, buf, strlen(buf), 0, (struct sockaddr *) &cli_add, len)< 0){
				perror("ERROR on first sendto");
				exit(1);
			}
			printf("Sent data port number %s to previous router\n", newport);
			
			// Create a new server UDP socket on the new data-port-number for listening 
			if ((newsd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0){
				perror("ERROR on creating socket");
				exit(1);
			}

			// zero out the structure
			memset((char *) &newserv_add, 0, sizeof(newserv_add));

			newserv_add.sin_family = AF_INET;
			newserv_add.sin_port = htons(newport);
			newserv_add.sin_addr.s_addr = htonl(INADDR_ANY);

			// bind socket to the new port 
			if (bind(newsd, (struct sockaddr *) &newserv_add, sizeof(newserv_add)) < 0){
				perror("ERROR on binding");
				exit(1);
			}
			printf("Server on new port %d...\n", newport);
			
			// Send a UDP packet containing stripped payload to the next router if not the last router
			if (count != 3){
				printf("Not the last router\n");
				
				// get the IP address of the next router
				char nextIP[20];
				temp = strrchr(stripped_buffer, '$'); 
				temp++;
				strcpy(nextIP, temp);
				printf("IP of the next router is %s\n", nextIP);
				printf("Lenght of next IP %ld\n", strlen(nextIP));
				
				// zero out the structure
				memset((char *) &cli_add, 0, sizeof(cli_add));

				if ((he=gethostbyname(nextIP)) == NULL) { // get the next router info
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
				
				printf("Sent stripped payload to the next router\n");
			}
			
			while(1){
				memset(buffer,0, BUFSIZE);
				memset((char *) &cli_add, 0, sizeof(cli_add));
				
				if ((n = recvfrom(newsd, buffer, BUFSIZE, 0, (struct sockaddr * ) &cli_add, &len)) == -1){
					perror("ERROR on recvfrom");
					exit(0);
				}
				printf("Message is %s\n from %s", buffer, inet_ntoa(cli_add.sin_addr));
				saved1 = saved2;
				saved2 = cli_add;

				if (strcmp (buffer, "terve") == 0){// send back to client 
					//printf("Yes, buffer %s\n", buffer);
					//printf("Sending packet to %s, %d\n",inet_ntoa(saved1.sin_addr), ntohs(saved1.sin_port));
					if (sendto(newsd, buffer, strlen(buffer), 0, (struct sockaddr *) &saved1, len) < 0){
						perror("ERROR on sendto\n");
						exit(1);
					}	
				}
				else{
					//printf("before send to real server, buffer %s\n", buffer);
					// Forward packet to the real server 
					//printf("Sending packet to %s, %d\n",inet_ntoa(rserv_add.sin_addr), ntohs(rserv_add.sin_port));

					if (sendto(newsd, buffer, strlen(buffer), 0, (struct sockaddr *) &rserv_add, len) < 0){
						perror("ERROR on sendto\n");
						exit(1);
					}
				}
				//printf("Check %d...\n", newport);

				// Check if receiving an end signal, i.e, a packet with payload of sie 3
				if (n == 3) {
					break;
				}
			}
		}
		else { // Parent process
			close(newsd);
			//close(sockd);
		}
	}

	// Close socket descriptor
	close(sd);
	return 0;
}
