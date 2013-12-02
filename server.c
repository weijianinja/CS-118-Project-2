#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <time.h> 
#include <fcntl.h>

int BUF_SIZE = 1024;

void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void get_resource (int requestfd, char* url) {

}

static void handle_request (char* request) {
    printf("Server received %d bytes", strlen(request), request);
}

int main(int argc, char *argv[]) {
    int listenfd = 0;
    struct sockaddr_in serv_addr, cli_addr; 
    int pid, clilen, portno, bytes_received, bytes_sent;
    char request[BUF_SIZE];

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>", argv[0]);
        exit(EXIT_FAILURE);
    }
    portno = atoi(argv[1]);

    /* creates unnamed socket inside the kernel and returns socket descriptor
    * AF_INET specifies IPv4 addresses
    * SOCK_STREAM specifies transport layer should be reliable
    * third argument is left 0 to default to TCP
    */
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listenfd < 0)
        error("ERROR opening socket");
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);

    // assigns the details specified in the structure ‘serv_addr’ to the socket created in the step above. The details include, the family/domain, the interface to listen on(in case the system has multiple interfaces to network) and the port on which the server will wait for the client requests to come.
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    //specifies maximum number of client connections that server will queue for this listening socket.
    listen(listenfd, 10);
    clilen = sizeof(cli_addr);

    // run in an infinite loop so that the server is always running and the delay or sleep of 1 sec ensures that this server does not eat up all of your CPU processing.
    while(1)
    {
        bytes_received = recvfrom(listenfd, request, BUF_SIZE, 0, (struct sockaddr*) &cli_addr, &clilen);
        if (bytes_received < 0)
            error("ERROR on receiving from client");
        printf("Connected to %s.\n", inet_ntoa(cli_addr.sin_addr));

        // each connection gets its own forked process
        pid = fork();
        if (pid < 0)
            error("ERROR on fork");
        if (pid == 0) {
            handle_request(request);

            // echo response for now
            bytes_sent = sendto(listenfd, request, strlen(request), 0, (struct sockaddr *) &cli_addr, clilen);
            if (bytes_sent < 0) 
              error("ERROR on sending");
            // end forked process
            exit(EXIT_SUCCESS);
        }
        else {
            waitpid(-1, NULL, WNOHANG);
        }
    }
}