#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdint.h>    
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <fcntl.h>  
#include <inttypes.h> 
#include "msg.h"


void Usage(char *progname);


int  Listen(char *portnum, int *sock_family);
void* HandleClient(void *arg);

int 
main(int argc, char **argv) 
{
  pthread_t thread;
  // Expect the port number as a command line argument.
  if (argc != 2) {
    Usage(argv[0]);
  }

  int sock_family;
  int listen_fd = Listen(argv[1], &sock_family);
  if (listen_fd <= 0) {
    // We failed to bind/listen to a socket.  Quit with failure.
    printf("Couldn't bind to any addresses.\n");
    return EXIT_FAILURE;
  }

  // Loop forever, accepting a connection from a client and doing
  // an echo trick to it.
  while (1) {
    struct sockaddr_storage caddr;
    socklen_t caddr_len = sizeof(caddr);
    int client_fd = accept(listen_fd,
                           (struct sockaddr *)(&caddr),
                           &caddr_len);
    if (client_fd < 0) {
      if ((errno == EINTR) || (errno == EAGAIN) || (errno == EWOULDBLOCK))
        continue;
      printf("Failure on accept:%s \n ", strerror(errno));
      break;
    }
     
    //Makes a thread to execute handler           
    pthread_create(&thread, NULL, HandleClient, &client_fd);
    //Return from handler to take next task
    pthread_join(thread, NULL);
  }

  // Close socket
  close(listen_fd);
  return EXIT_SUCCESS;
}

void Usage(char *progname) {
  printf("usage: %s port \n", progname);
  exit(EXIT_FAILURE);
}


int 
Listen(char *portnum, int *sock_family) {

  // Populate the "hints" addrinfo structure for getaddrinfo().
  // ("man addrinfo")
  struct addrinfo hints;
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;       // IPv6 (also handles IPv4 clients)
  hints.ai_socktype = SOCK_STREAM;  // stream
  hints.ai_flags = AI_PASSIVE;      // use wildcard "in6addr_any" address
  hints.ai_flags |= AI_V4MAPPED;    // use v4-mapped v6 if no v6 found
  hints.ai_protocol = IPPROTO_TCP;  // tcp protocol
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  // getaddrinfo() returns a list of
  // address structures via the output parameter "result".
  struct addrinfo *result;
  int res = getaddrinfo(NULL, portnum, &hints, &result);

  // Did addrinfo() fail?
  if (res != 0) {
	printf( "getaddrinfo failed: %s", gai_strerror(res));
    return -1;
  }

  // Loop through the returned address structures until we are able
  // to create a socket and bind to one.  The address structures are
  // linked in a list through the "ai_next" field of result.
  int listen_fd = -1;
  struct addrinfo *rp;
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    listen_fd = socket(rp->ai_family,
                       rp->ai_socktype,
                       rp->ai_protocol);
    if (listen_fd == -1) {
      // Creating this socket failed.  So, loop to the next returned
      // result and try again.
      printf("socket() failed:%s \n ", strerror(errno));
      listen_fd = -1;
      continue;
    }

    // Configure the socket; we're setting a socket "option."  In
    // particular, we set "SO_REUSEADDR", which tells the TCP stack
    // so make the port we bind to available again as soon as we
    // exit, rather than waiting for a few tens of seconds to recycle it.
    int optval = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR,
               &optval, sizeof(optval));

    // Try binding the socket to the address and port number returned
    // by getaddrinfo().
    if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      // Bind worked!  Print out the information about what
      // we bound to.
      //PrintOut(listen_fd, rp->ai_addr, rp->ai_addrlen);

      // Return to the caller the address family.
      *sock_family = rp->ai_family;
      break;
    }

    // The bind failed.  Close the socket, then loop back around and
    // try the next address/port returned by getaddrinfo().
    close(listen_fd);
    listen_fd = -1;
  }

  // Free the structure returned by getaddrinfo().
  freeaddrinfo(result);

  // If we failed to bind, return failure.
  if (listen_fd == -1)
    return listen_fd;

  // Success. Tell the OS that we want this to be a listening socket.
  if (listen(listen_fd, SOMAXCONN) != 0) {
    printf("Failed to mark socket as listening:%s \n ", strerror(errno));
    close(listen_fd);
    return -1;
  }

  // Return to the client the listening file descriptor.
  return listen_fd;
}


void* HandleClient(void* arg) 
{
  int8_t EXIT = 0;
  int c_fd = *(int *)arg;
  // Print out information about the client.
  printf("\nNew client connection \n" );

  // Loop, reading data and echo'ing it back, until the client
  // closes the connection.
  while (1) {
    
    //received_msg to hold messge from client
    struct msg received_msg;
    
    //Reads message from client
    ssize_t res = read(c_fd, &received_msg, sizeof(struct msg));
    if (res == 0) {
      printf("[The client disconnected.] \n");
      pthread_exit(NULL);
    }

    //Checks if theres an error in the socket
    if (res == -1) {
      if ((errno == EAGAIN) || (errno == EINTR))
        continue;

	    printf(" Error on client socket:%s \n ", strerror(errno));
      pthread_exit(NULL);
    }
    
    //Opens database for use and stores in fd
    int32_t fd = open( "str", O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
    
    //Prints an error if the database doesn't open while running
    if (fd == -1) {
      printf("Failure while opening the database\n");
      received_msg.type = FAIL;
      write(c_fd, &received_msg, sizeof(struct msg));
    }
    
    //If in the case an xxeit message is received
    if ( received_msg.type == EXIT ) {
      printf("[The client disconnected.] \n");
      pthread_exit(NULL);
    
    //If the client inputted PUT then the following is executed
    } else if ( received_msg.type == PUT ) {
      
      //A struct record data is made to check for duplicates
      struct record data;
      ssize_t nread;
      
      //Loops through database and checks if the user inputted id is taken
      while (( nread = read(fd, &data, sizeof(struct record))) > 0 ){
        if (data.id == received_msg.rd.id){
          //Sends fail message if there is a duplicate
          received_msg.type = FAIL;
          write(c_fd, &received_msg, sizeof(struct msg));
          //Ends thread
          pthread_exit(NULL);
        
        }
      }
      //Writes the new data into the database if unique
      write(fd, &received_msg.rd, sizeof(struct record));
      
      //Sends success message to client and ends threads
      received_msg.type = SUCCESS;
      write(c_fd, &received_msg, sizeof(struct msg));
      pthread_exit(NULL);
    
    //If the client inputted GET then the following is executed
    } else if ( received_msg.type == GET ) {
    
      //Struct record data to check for uniqueness 
      struct record data;
      ssize_t nread;
      int retrieved = 0;
      
      //loops through the database and checks if the user inputted id exists in database
      while (( nread = read(fd, &data, sizeof(struct record))) > 0 ){
        if (data.id == received_msg.rd.id){
          received_msg.rd = data;   
          retrieved = 1;   
        }        
      }
      
      //If nothing was found, then server returns error message back to user
      if(retrieved == 0){
        received_msg.type = FAIL;
        write(c_fd, &received_msg, sizeof(struct msg));
        pthread_exit(NULL);
      }
  
      //If no name exists then an error is ent and program ends 
    	if(received_msg.rd.name[0] == '\0'){
    		printf("Record has not been put\n");
    		received_msg.type = FAIL;
        write(c_fd, &received_msg, sizeof(struct msg));
        pthread_exit(NULL);
    	}
     
      //Returns success message and retrieved data 
      received_msg.type = SUCCESS;
      write(c_fd, &received_msg, sizeof(struct msg));
      pthread_exit(NULL);

    
    } else {
      //Returns false message if anything else was inputted
      received_msg.type = FAIL;
      write(c_fd, &received_msg, sizeof(struct msg));
      pthread_exit(NULL);
    
    }

    // Really should do this in a loop in case of EAGAIN, EINTR,
    // or short write, but I'm lazy.  Don't be like me. ;)
    //write(c_fd, "You typed: ", strlen("You typed: "));
    //write(c_fd, clientbuf, strlen(clientbuf));
  }

  close(c_fd);
}

