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

int token_payload(char * buffer, char * tokens[]){
	int i = 0;
	char * temp; 
	temp = strtok(buffer, "$");
	while (temp != NULL){
		tokens[i++] = temp;
		printf("%s\n", temp);
		temp = strtok(NULL, "$");
	}
	
	return i--;
}

// function to check whether it is the first router
int isFirst(char * ptr){
	char * hostname;
	char * ip;
	hostname = (char *) malloc(50);
	ip = (char *) malloc(10);
	struct hostent * he;
	struct in_addr **addr_list;
	int i;
	
	gethostname(hostname, sizeof(hostname));
	printf("Host name is %s\n", hostname);
	
	if ((he = gethostbyname()) == NULL){
		perror("gethostbyname");
	}
	
	addr_list = (struct in_addr **) he->h_addr_list;
	for(i = 0; addr_list[i] != NULL; i++) {
		//Return the first one;
		strcpy(ip , inet_ntoa(*addr_list[i]) );
		break;
	}
	
	printf("IP address is %s\n", ip);
	
	if (strcmp(ip, ptr) == 0){
		return 1;
	}
	
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
	isFirst("127.0.0.1");
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

		int pid = fork();
		if (pid < 0){
			perror("ERROR on fork");
		}
		else if (pid == 0){
			serverPort = strtok(buffer,"$");
			serverIP = strtok(NULL, "$");
			printf("Received request from %s, %d\n", serverIP, atoi(serverPort));
			// zero out the structure
			memset((char *) &rserv_add, 0, sizeof(rserv_add));

			if ((he=gethostbyname(serverIP)) == NULL) { // get the host info
				perror("gethostbyname");
				exit(1);
			}

			rserv_add.sin_family = AF_INET;
			rserv_add.sin_port = htons(atoi(serverPort));
			rserv_add.sin_addr = *((struct in_addr *)he->h_addr);
			bzero(&(rserv_add.sin_zero), 8); // zero the rest of the struct

			srand(time(NULL));
			newport = 10000 + rand()%20000;
			memset(buf, 0, BUFSIZE);
			sprintf(buf, "%d", newport);

			// Send a UDP packet, containing data-port-number, back to overlaybuild
			if (sendto(sd, buf, strlen(buf), 0, (struct sockaddr *) &cli_add, len)< 0){
				perror("ERROR on first sendto");
			exit(1);
			}
			// Create a new server UDP socket 
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
			printf("Server on port %d...\n", newport);

			while(1){
				memset(buffer,0, BUFSIZE);
				if ((n = recvfrom(newsd, buffer, BUFSIZE, 0, (struct sockaddr * ) &cli_add, &len)) == -1){
					perror("ERROR on recvfrom");
					exit(0);
				}

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