#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include "packet.c"

int BUF_SIZE = 1024;
double LOSS_PROB = 0.4;
double CORRUPT_PROB = 0.3;

/* 
 * error - wrapper for perror
 */
void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

double random_num() {
    return (double) rand()/(double) RAND_MAX;
}

int main(int argc, char **argv) {
    int socketfd, resourcefd, portno, n, bytes_received, expected_seq_no;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname, *filename;
    char buf[BUF_SIZE];

    /* check command line arguments */
    if (argc != 4) {
       fprintf(stderr,"usage: %s <hostname> <port> <filename>\n", argv[0]);
       exit(EXIT_FAILURE);
    }
    hostname = argv[1];
    portno = atoi(argv[2]);
    filename = argv[3];

    /* socket: create the socket */
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0) 
        error("ERROR opening socket");

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        exit(EXIT_FAILURE);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* build the request */
    struct packet req_pkt;
    bzero((char *) &req_pkt, sizeof(req_pkt));
    memcpy(req_pkt.data, filename, strlen(filename));
    req_pkt.length = sizeof(req_pkt.type) * 3 + strlen(filename);

    /* send the request to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(socketfd, &req_pkt, req_pkt.length, 0, (struct sockaddr*) &serveraddr, serverlen);
    if (n < 0)
      error("ERROR in sendto");
    
    struct packet rspd_pkt;
    int packet_loss, packet_corruption;
    rspd_pkt.length = 1;
    bytes_received = 0;
    resourcefd = open(filename, O_WRONLY);

    bzero((char *) &req_pkt, sizeof(req_pkt));
    rspd_pkt.length = sizeof(int) * 3;

    while (bytes_received < rspd_pkt.length) {
        packet_loss = LOSS_PROB < random_num();
        packet_corruption = CORRUPT_PROB < random_num();
        if (recvfrom(socketfd, &rspd_pkt, sizeof(rspd_pkt), 0, (struct sockaddr*) &serveraddr, (socklen_t*) &serverlen) > 0 && !packet_loss) {
            if (rspd_pkt.seq_no == expected_seq_no && !packet_corruption) {
                write(resourcefd, rspd_pkt.data, strlen(rspd_pkt.data));
                expected_seq_no++;
            }
            req_pkt.type = 1;
            req_pkt.seq_no = expected_seq_no;
            n = sendto(socketfd, &req_pkt, req_pkt.length, 0, (struct sockaddr*) &serveraddr, serverlen);
            if (n < 0)
                error("ERROR in sendto");
        }
    }
    return 0;
}