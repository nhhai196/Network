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

int main(int argc, char * argv[]){
	// Variable declrations and initializations
	int udp_sd, tcp_sd;
	int tcp_port, udp_port, new_tcp_sd;
	int len;
	int playload_size, playback_del, gamma, buf_sz, target_buf;
	char filename[20];
	struct sockaddr_in tserv_add, userv_add, tcli_add, ucli_add;
	struct hostent * he; 
	
	// get the size of struct addr
	len = sizeof(my_add);

	// Check whether thu number of input arguments is correct
	if (argc != 9){
		printf("Usage: ./audiolisten server-ip server-tcp-port client-udp-port payload-size playback-del gamma buf-sz target-buf logfile-c filename

\n");
		exit(0);
	}
	
	// Convert Inputs
	if ((he = gethostbyname(argv[1])) == NULL) {
		printf("ERROR: the host name %s doesn't exist\n", argv[1]);
		exit(0);
	}
	
	tcp_port = atoi(argv[2]);
	udp_port = atoi(arbv[3]);
	payload_size = atoi(argv[4]);
	packet_spacing = atoi(argv[5]);
	palyback_del = atoi(argv[6]);
	gamma = atoi(argv[7]);
	buf_sz = atoi(argv[8]);
	target_buf = atoi(argv[9]);
	strcpy(filename, argv[10], strlen(argv[10]));
	
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
	tserv_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(tserv_addr.sin_zero), 8); // zero the rest of the struct

	if (connect(tcp_sd, (struct sockaddr *)&tserv_addr, sizeof(struct sockaddr)) == -1) {
		perror("connect");
		exit(1);
	}


