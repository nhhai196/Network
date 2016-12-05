// client using UDP

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
#define BUFSIZE 1024

void generate_pad(int keylen, char s[]){
  static const char alphanum[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
  int i, len;
  srand ( time(NULL) );

  // Find the length of the pad
  len = 1000 - keylen;
  for (i = 0; i < len; ++i) {
    s[i] = alphanum[rand() % (sizeof(alphanum) - 1)];
  }

  s[len] = 0;
}

double elapsed_time(struct timeval start, struct timeval end){
  double result, sec, usec;
  sec = (end.tv_sec - start.tv_sec);	
  usec = (end.tv_usec - start.tv_usec);
  //printf("sec is %lld, usec is %lld\n", sec, usec);
  result =  sec * 1000 + usec/1000; 
	   
  return result;
}


void handle_alarm( int sig ) {
    signal(sig, SIG_IGN);
    printf("No response from ping server\n");
    exit(1);
}

int main(int argc, char * argv[]){
  struct sockaddr_in serv_add;
  char buffer[BUFSIZE];
  int sd, n;
  socklen_t len;
  char pad[1010];
  struct timeval start, end;
  	
  if (argc != 4){
    printf("Usage: ./myping ipaddress portnumber secretkey\n");
    exit(0);
  } 
 
  len = sizeof(serv_add);

  // Create a UDP socket
  if ((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
    perror("ERROR on creating socket");
    exit(1);
  }

  // zero out the structure
  memset((char *) &serv_add, 0, sizeof(serv_add));

  serv_add.sin_family = AF_INET;
  serv_add.sin_port = htons(atoi(argv[2]));
  serv_add.sin_addr.s_addr = htonl(INADDR_ANY);
  if (inet_aton(argv[1], &serv_add.sin_addr) == 0){
    perror("ERROR on inet_aton\n");
    exit(1);
  }

  // Zero out buffer
  memset(buffer, 0, sizeof(buffer));

  // Create a request
  sprintf(buffer, "$%s$", argv[3]); 
  generate_pad(strlen(buffer), pad);
  strcat(buffer, pad);
  printf("A request is created. First 50 characters of the request:\n%*.*s\n", 50, 50, buffer);
  
  // Get the time right before sending to the server
  signal(SIGALRM, handle_alarm);
  alarm(2.55); // Go off after 2.55 seconds
  gettimeofday(&start, NULL);

  // Send to the server
  if (sendto(sd, buffer, strlen(buffer), 0, (struct sockaddr *) &serv_add, len) < 0){
    perror("ERROR on send to\n");
    exit(1);
  }

  // Receive respons from the server
  memset(buffer, 0, BUFSIZE);
  if ((n = recvfrom(sd, buffer, BUFSIZE, 0, (struct sockaddr * ) &serv_add, &len))< 0){
    perror("ERROR on recvfrom\n");
    exit(1);
  }

  //signal(SIGALRM, ALARMhandler);
  // Check if the message recived is correct	
  if (strcmp(buffer, "terve") != 0){
    printf("Not the correct message!\n");
  }
  else {
    printf("Get the correct message from server: %s\n", buffer);
  }

  // Get the time right after receiving respsonse
  //sleep(1);
  gettimeofday(&end, NULL);
  printf("Elapsed time = %.2f ms\n", elapsed_time(start, end));

  close(sd);

  return 0;
}
  
	
		 			
