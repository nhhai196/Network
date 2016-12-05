// server using UDP

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include<arpa/inet.h>
#include<sys/socket.h>
#define BUFSIZE 1001

int main(int argc, char * argv[]){
  struct sockaddr_in serv_add, cli_add;
  char buffer[BUFSIZE];
  int sd, n, l;
  socklen_t len;
  char * secretkey;
  char * pad;
  int request_num = 0;

  // get the size of struct addr
  len = sizeof(cli_add);

  if (argc != 3){
    printf("Usage: ./mypingd portnumber secretkey\n");
    exit(0);
  }

  // check the srecretkey length
  l = strlen(argv[2]);
  if (l < 10 || l > 20){
    printf("Secretkey is too short or too long\n");
    exit(1);
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

  // Keep listening for data
  while(1) {
    // Check the number of requests 
    if (request_num >= 3){
      printf("The maximum number of requests has reached.\nServer will ignore every 4th client request.\n");
      break;
    }

    printf("Waiting for data...");
    fflush(stdout);
    memset(buffer, 0, BUFSIZE);

    if ((n = recvfrom(sd, buffer, BUFSIZE, 0, (struct sockaddr * ) &cli_add, &len)) == -1){
      perror("ERROR on recvfrom");
      exit(0);
    }
    // Update the number of requests received so far
    request_num++;

    // Print out the details of the client 
    printf("Received request from %s\n", inet_ntoa(cli_add.sin_addr)); // ntohs(cli_add.sin_port));
    printf("50 initial characters of received data:\n%*.*s\n", 50, 50, buffer);

    // Get the secretkey
    int buflen = strlen(buffer);
    secretkey = strtok(buffer, "$");
    pad = strtok(NULL, "$");

    // Check the secretkey and the length of received message
    if (strcmp (secretkey, argv[2]) != 0 || buflen != 1000){
      printf("ERROR on key length or pad\n");
      printf("**************************************************\n");
      continue;
    }
    else{
      // For testing signal arlarm only
      //sleep(3);
      
      // Reply to the client
      if (sendto(sd, "terve", sizeof("terve"), 0, (struct sockaddr *) &cli_add, len) < 0){
	perror("ERROR on sendto\n");
	exit(1);
      }
    }

    // For pretty printout 
    printf("**************************************************\n");
  }

  close(sd);
  return 0;
}
