/* A traffic generator app
 * traffic_snd.c
 * @author Hai Nguyen 
 */



#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <signal.h>
#include <time.h>
#include <netdb.h>
#define BUFSIZE 1024

// Function to compute elapsed time in seconds
double elapsed_time(struct timeval start, struct timeval end){
  double result, sec, usec;
  sec = (end.tv_sec - start.tv_sec);
  usec = (end.tv_usec - start.tv_usec);

  result =  sec + usec/1000000;

  return result;
}


int main(int argc, char * argv[]){
  // Variable declrations and initializations
  struct sockaddr_in serv_add, my_addr;
  char buffer[BUFSIZE];
  int sd, n, i;
  double bps, pps;
  socklen_t len;
  struct timeval start, end;
  struct hostent *he;

  // Check if the number of arguments is correct
  if (argc != 6){
    printf("Usage: ./traffic_snd IP-address port-number payload-size packet-count packet-spacing\n");
    exit(0);
  }

  // Variables and Innitializations from user inputs
  unsigned long payload_size = atoi(argv[3]);
  unsigned long packet_count = atoi(argv[4]);
  unsigned long packet_spacing = atoi(argv[5]);
  char payload[payload_size] ;
  // Ethernet 14 bytes(header) + 4 bytes (trailer) + UDP headers 8 bytes + payload_size bytes  
  unsigned long packet_size = payload_size + 26;
  printf("payload_size, packet_count: %ld, %ld\n", payload_size, packet_count);
  len = sizeof(serv_add);

  // Create a UDP socket
  if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    perror("ERROR on creating socket");
    exit(1);
  }
  
	// zero out the structure
	memset((char *) &my_addr, 0, sizeof(my_addr));

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(11111);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

	// bind socket to the build-port 
	if (bind(sd, (struct sockaddr *) &my_addr, sizeof(my_addr)) < 0){
		perror("ERROR on binding");
		exit(1);
	}

  if ((he=gethostbyname(argv[1])) == NULL) { // get the host info
    perror("gethostbyname");
    exit(1);
  }
  // zero out the structure
  memset((char *) &serv_add, 0, sizeof(serv_add));

  serv_add.sin_family = AF_INET;
  serv_add.sin_port = htons(atoi(argv[2]));
  serv_add.sin_addr = *((struct in_addr *)he->h_addr);
  bzero(&(serv_add.sin_zero), 8); // zero the rest of the struct


  // Zero out buffer
  memset(buffer, 0, sizeof(buffer));

  // Fill the payload with the first letter of my last name
  bzero(payload, payload_size);
  for (i = 0; i < payload_size; i++){
    strcat(payload, "N");
  } 
  
  // Time stamp before sending packets (entering for loop)
  gettimeofday(&start, NULL);

  /* A for loop to send packet_count packets, the payload of each packet is 
     the first letter of my last name "N" */
  for (i = 0; i < packet_count ; i ++){
    if (sendto(sd, payload, payload_size, 0, (struct sockaddr *) &serv_add, len) < 0){
      perror("ERROR on send to\n");
      exit(1);
    }

    // Using usleep to set up the delay between seccessive sendto()
    usleep(packet_spacing);
  }

  // Time stamp after finishing (exiting for loop)
  gettimeofday(&end, NULL);
 
  // Compute and print to stdout the completion time, bit rate in units of bps and pps  
  double time = elapsed_time(start, end);
  printf("The completion time is: %.6f s\n", time);
  pps = packet_count/time;
  bps = packet_size * pps;
  printf("Bit rates: %.3f Bps, %.0f pps\n", bps, pps); 

  // Signal end of transmission to the receiver by transmitting 3 packets,
  // each of payload 3 bytes without pause 
  for (i = 0; i < 3; i++){
    if (sendto(sd, "NNN", 3, 0, (struct sockaddr *) &serv_add, len) < 0){
      perror("ERROR on send to\n");
      exit(1);
    }
  }     

  // Close socket descriptor
  close(sd);

  return 0;
}
