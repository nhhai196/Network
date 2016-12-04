/* overlaybuild.c
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
#include <sys/time.h>

#define BUFFSIZE 1024

int main(int argc, char * argv[]){
	// Variable declrations and initializations
	struct sockaddr_in their_addr;
	char buffer[BUFFSIZE];
	int sd, n, i;
	socklen_t len;
	struct timeval start, end;
	struct hostent *he;

	len = sizeof(their_addr);
	
	// Check whether the number of input arguments is correct
	if (argc < 6){
		printf("Usage: ./overlaybuild dst-IP dst-port routerk-IP ... router2-IP router1-IP overlay-port build-port\n");
		exit(0);
	}

	// Create a UDP socket
	if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("ERROR on creating socket");
		exit(1);
	}

	// zero out the structure
	memset((char *) &their_addr, 0, sizeof(their_addr));

	if ((he=gethostbyname(argv[1])) == NULL) { // get the host info
	perror("gethostbyname");
	exit(1);
	}

	their_addr.sin_family = AF_INET;
	their_addr.sin_port = htons(atoi(argv[argc-2]));
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(their_addr.sin_zero), 8); // zero the rest of the struct

	// Zero out buffer
	memset(buffer, 0, BUFFSIZE);
	
	// Create payload $dst-IP$dst-port$routerk-IP$...$router2-IP$router1-IP$
	sprintf(buffer, "$%s$%s", argv[1], argv[2]);
	int count = 3; 
	for (count = 3; count < argc - 2; count++){
		strcat(buffer, "$");
		strcat(buffer, argv[count]);
	}
	
	printf("Buffer is %s\n", buffer);

	// Time stamp before sending packets (entering for loop)
	gettimeofday(&start, NULL);

	// send packet to overlayrouter
	if (sendto(sd, buffer, strlen(buffer), 0, (struct sockaddr *) &their_addr, len) < 0){
		perror("ERROR on send to\n");
		exit(1);
	}
	 
	// Receive respons from overlayrouter
	memset(buffer, 0, BUFFSIZE);
	if ((n = recvfrom(sd, buffer, BUFFSIZE, 0, (struct sockaddr * ) &their_addr, &len))< 0){
		perror("ERROR on recvfrom\n");
		exit(1);
	}

	printf("The data-port-1: %s\n", buffer);

	// Close socket descriptor
	close(sd);

	return 0; 
}
