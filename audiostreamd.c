/* audiostreamd.c
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

int main(int argc, char * argv[]){
	// Variable declrations and initializations
	int udp_sd, tcp_sd;
	int tcp_port, udp_port, new_tcp_sd;
	int len;
	int playload_size, packet_spacing, mode;
	struct sockaddr_in tserv_add, userv_add, tcli_add, ucli_add;
	
	// get the size of struct addr
	len = sizeof(my_add);

	// Check whether thu number of input arguments is correct
	if (argc != 7){
		printf("Usage: audiostreamd tcp-port udp-port payload-size packet-spacing mode logfile-s 
\n");
		exit(0);
	}
	
	// Convert Inputs
	tcp_port = atoi(argv[1]);
	udp_port = atoi(arbv[2]);
	payload_size = atoi(argv[3]);
	packet_spacing = atoi(argv[4]);
	mode = atoi(argv[5]);
	
	
	//Create a TCP socket 
	tcp_sd = socket(AF_INET, SOCK_STREAM, 0);

	if (tcp_sd < 0) {
		printf("ERROR opening socket\n");
		exit(1);
	}
	
	// Initialize socket struc
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
	listen(sd, 5);
	
	while(1) {
		//printf("Waiting for data...");
		new_tcp_sd = accept(tcp_sd, (struct sockaddr *) &tcli_add, &len);
		if (newsd < 0) {
			printf("ERROR on accept\n");
		}
		// Create child process
		pid = fork();

		if (pid <0) {
			printf("ERROR on fork\n");
			exit(1);
		}
	
		if (pid == 0) { // Child process 
		handle_single_client(newsd, argv[2], cli_add);
		}
		else { // Parent process
		close(newsd);
		printf("***************************************\n");
		}
	}
	
	// Create a UDP socket
	if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
		perror("ERROR on creating socket");
		exit(1);
	}

	// zero out the structure
	memset((char *) &my_add, 0, sizeof(my_add));

	my_add.sin_family = AF_INET;
	my_add.sin_port = htons(atoi(argv[1]));
	my_add.sin_addr.s_addr = htonl(INADDR_ANY);
	bzero(&(my_add.sin_zero), 8); // zero the rest of the struct

	// bind socket to port 
	if (bind(sd, (struct sockaddr *) &my_add, sizeof(my_add)) < 0){
		perror("ERROR on binding");
		exit(1);
	}
	
}

