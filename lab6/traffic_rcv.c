/* A traffic generator app
* traffic_rcv.c
* @author Hai Nguyen 
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#include <sys/time.h>

// Function to compute elapsed time in seconds
double elapsed_time(struct timeval start, struct timeval end){
  double result, sec, usec;
  sec = (end.tv_sec - start.tv_sec);	
  usec = (end.tv_usec - start.tv_usec);
  //printf("sec is %lld, usec is %lld\n", sec, usec);
  result =  sec + usec/1000000; 
	   
  return result;
}

int main(int argc, char * argv[]){
  // Variable declrations and initializations
  struct sockaddr_in serv_add, cli_add;
  int sd, n, l, payload_size;
  int packet_count = 0;
  socklen_t len;
  int request_num = 0;
  struct timeval start, end;
  unsigned long total_bytes = 0;
  unsigned long payload, packet_size;
  double bps, pps;

  // get the size of struct addr
  len = sizeof(cli_add);

  // Check whether thu number of input arguments is correct
  if (argc != 3){
    printf("Usage: ./traffic_rcv port-number payload-size\n");
    exit(0);
  }

  // Get payload size from input, and declare a buferr
  payload_size = atoi(argv[2]);
  char buffer[payload_size];

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
    memset(buffer, 0, payload_size);

    if ((n = recvfrom(sd, buffer, payload_size, 0, (struct sockaddr * ) &cli_add, &len)) == -1){
      perror("ERROR on recvfrom");
      exit(0);
    }
    
    // Check if receiving an end signal, i.e, a packet with payload of sie 3
    if (n == 3) {
      gettimeofday(&end, NULL);
      break;
    }
    else{
      // Update the number of packet and bytes received so far
      packet_count++;
      total_bytes += n; 

      // If the first packet, get the time stamp
      if (packet_count == 1) {
        gettimeofday(&start, NULL);
      }
    }
  }

  // Compute and print to stdout the completion time, bit rate in units of bps and pps
  double time = elapsed_time(start, end);
  printf("The completion time is: %.6f s\n", time);
  pps = packet_count/time;
  packet_size = total_bytes/packet_count + 26;
  bps = packet_size * pps;
  printf("Bit rates: %.3f Bps, %.0f pps\n", bps, pps); 

  // Close socket descriptor
  close(sd);
  return 0;
}
