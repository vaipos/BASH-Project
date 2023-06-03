#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <inttypes.h>
#include <sys/types.h>
#include "msg.h"

#define BUF 256

void Usage(char *progname);

int LookupName(char *name,
                unsigned short port,
                struct sockaddr_storage *ret_addr,
                size_t *ret_addrlen);

int Connect(const struct sockaddr_storage *addr,
             const size_t addrlen,
             int *ret_fd);

int main(int argc, char **argv) {
  printf("hi\n");
  if (argc != 3) {
    Usage(argv[0]);
  }

  unsigned short port = 0;
  if (sscanf(argv[2], "%hu", &port) != 1) {
    Usage(argv[0]);
  }
  

  // Get an appropriate sockaddr structure.
  struct sockaddr_storage addr;
  size_t addrlen;
  if (!LookupName(argv[1], port, &addr, &addrlen)) {
    Usage(argv[0]);
  }
  
 

  // Connect to the remote host.
  int socket_fd;
  if (!Connect(&addr, addrlen, &socket_fd)) {
    Usage(argv[0]);
  }
  


  // Read something from the remote host.
  // Will only read BUF-1 characters at most.
  
  //sending_msg to get the message from server
  struct msg sending_msg;
  
  //receiving_msg to receive message from server
  struct msg receiving_msg;
  int res;

  //While loop to continuously write to server
  while (1) {
  
    //Int choice for user input
    int8_t choice;
    
    //int8_t to hold EXIT value
    int8_t EXIT = 0;
    
    
    //While loop to keep asking for user input
    int flag = 1;
    while(flag){
      printf("Enter your choice (1 to put, 2 to get, 0 to quit): ");
      //Takes in user input
      scanf("%" SCNd8 "%*c", &choice);
      
      //If the user inputted 0, then the client closes the socket and exits
      if ( choice == EXIT ){
        sending_msg.type = EXIT;
        close(socket_fd);
        return EXIT_SUCCESS;
        break;
      
      //If the user inputted 1, the program will ask to input a name and id to store in the database
      } else if ( choice == PUT){
        //Makes sending_msg type PUT so server can execute put
        sending_msg.type = PUT;
        
        //Asks for the name and stores in sending_msg
        printf("Enter the name: ");	
        fgets(sending_msg.rd.name, MAX_NAME_LENGTH, stdin);
	      sending_msg.rd.name[strlen(sending_msg.rd.name)-1] = '\0';
             
        //Asks for the id and stores in sending_msg 
        printf("Enter the id: ");
        scanf("%d", &sending_msg.rd.id);
        //Sets flag to 0 to not ask for input
        flag = 0;
      
      //If the user inputted 2, the program will ask to input a id to find in the database
      } else if (choice == GET ){
        //Makes sending_msg type GET so server can execute get
        sending_msg.type = GET;
        
        //Asks for the id and stores in sending_msg 
        printf("Enter the student id: ");
        scanf("%d", &sending_msg.rd.id);
        
        //Sets flag to 0 to not ask for input
        flag = 0;
      
      //If anything else, the program asks for more input
      } else {
        printf("Incorrect input, try again.\n");
      }
    }
    
    //Writes to server so server can execute specified function
    int wres = write(socket_fd, &sending_msg, sizeof(struct msg));
    
    //Checks if the socket was close prematurely
    if (wres == 0) {
     printf("socket closed prematurely \n");
      close(socket_fd);
      return EXIT_FAILURE;
    }
    
    //Checks if socket write was a failure
    if (wres == -1) {
      if (errno == EINTR)
        continue;
      printf("socket write failure \n");
      close(socket_fd);
      return EXIT_FAILURE;
    }
    break;
  }
  
  //While loop to keep receiving messages from server
  while (1) {
  
    //Reads the message sent from the server
    res = read(socket_fd, &receiving_msg, sizeof(struct msg));
    
    //Checks if socket was closed prematurely
    if (res == 0) {
      printf("socket closed prematurely \n");
      close(socket_fd);
      return EXIT_FAILURE;
    }
    
    //Checks if the socket read was a failure
    if (res == -1) {
      if (errno == EINTR)
        continue;
      printf("socket read failure \n");
      close(socket_fd);
      return EXIT_FAILURE;
      
    }
    //Checks to see if the user executed PUT or GET
    if ( sending_msg.type == PUT){
      //Checks message from server to see if put was successful
      if ( receiving_msg.type == SUCCESS) {
        //Print that put was a success if it was
        printf("put success\n"); 
      } else if (receiving_msg.type == FAIL){
      //Print that put was a failure if it was
        printf("put failed\n");
      }
    } else if ( sending_msg.type == GET) {
      //Checks message from server to see if put was successful
      if ( receiving_msg.type == SUCCESS) {
        //Prints the name and id found in database if successful
        printf("name %s \n", receiving_msg.rd.name);	
	      printf("id: %d \n", receiving_msg.rd.id);
      } else if (receiving_msg.type == FAIL){
        //Print that get was a failure if it was
        printf("get failed\n");
      }
    }
    break;
  }
  
  
  

  // Clean up.
  close(socket_fd);
  return EXIT_SUCCESS;
}

void 
Usage(char *progname) {
  printf("usage: %s  hostname port \n", progname);
  exit(EXIT_FAILURE);
}

int 
LookupName(char *name,
                unsigned short port,
                struct sockaddr_storage *ret_addr,
                size_t *ret_addrlen) {
  struct addrinfo hints, *results;
  int retval;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;

  // Do the lookup by invoking getaddrinfo().
  if ((retval = getaddrinfo(name, NULL, &hints, &results)) != 0) {
    printf( "getaddrinfo failed: %s", gai_strerror(retval));
    return 0;
  }

  // Set the port in the first result.
  if (results->ai_family == AF_INET) {
    struct sockaddr_in *v4addr =
            (struct sockaddr_in *) (results->ai_addr);
    v4addr->sin_port = htons(port);
  } else if (results->ai_family == AF_INET6) {
    struct sockaddr_in6 *v6addr =
            (struct sockaddr_in6 *)(results->ai_addr);
    v6addr->sin6_port = htons(port);
  } else {
    printf("getaddrinfo failed to provide an IPv4 or IPv6 address \n");
    freeaddrinfo(results);
    return 0;
  }

  // Return the first result.
  assert(results != NULL);
  memcpy(ret_addr, results->ai_addr, results->ai_addrlen);
  *ret_addrlen = results->ai_addrlen;

  // Clean up.
  freeaddrinfo(results);
  return 1;
}

int 
Connect(const struct sockaddr_storage *addr,
             const size_t addrlen,
             int *ret_fd) {
  // Create the socket.
  int socket_fd = socket(addr->ss_family, SOCK_STREAM, 0);
  if (socket_fd == -1) {
    printf("socket() failed: %s", strerror(errno));
    return 0;
  }

  // Connect the socket to the remote host.
  int res = connect(socket_fd,
                    (const struct sockaddr *)(addr),
                    addrlen);
  if (res == -1) {
    printf("connect() failed: %s", strerror(errno));
    return 0;
  }

  *ret_fd = socket_fd;
  return 1;
}
