#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include "packet.c"

int BUF_SIZE = 1024;
double LOSS_PROB = 0.00;
double CORRUPT_PROB = 0.1;

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
    int socketfd, portno, n, expected_seq_no;
    int serverlen;
    struct sockaddr_in serveraddr;
    struct hostent *server;
    char *hostname, *filename;
    char buf[BUF_SIZE];
    FILE* resource;

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
    strcpy(req_pkt.data, filename);
    req_pkt.length = sizeof(req_pkt.type) * 3 + strlen(filename) + 1;

    /* send the request to the server */
    serverlen = sizeof(serveraddr);
    n = sendto(socketfd, &req_pkt, req_pkt.length, 0, (struct sockaddr*) &serveraddr, serverlen);
    if (n < 0)
      error("ERROR in sendto");
    printf("Requested file %s\n", req_pkt.data);

    struct packet rspd_pkt;
    rspd_pkt.length = DATA_SIZE;
    expected_seq_no = 1;
    resource = fopen(strcat(filename, "_copy"), "wb");

    bzero((char *) &req_pkt, sizeof(req_pkt));
    req_pkt.length = sizeof(int) * 3;
    req_pkt.type = 1;
    req_pkt.seq_no = expected_seq_no - 1;

    srand(time(NULL));
    while (1) {
        if (recvfrom(socketfd, &rspd_pkt, sizeof(rspd_pkt), 0, (struct sockaddr*) &serveraddr, (socklen_t*) &serverlen) < 0 || random_num() < LOSS_PROB) {
            printf("Packet lost!\n");
        }
        else if (random_num() < CORRUPT_PROB) {
            printf("Packet corrupted!\n");
            if (sendto(socketfd, &req_pkt, req_pkt.length, 0, (struct sockaddr*) &serveraddr, serverlen) < 0)
                error("ERROR responding to corrupt packet");
        }
        else {
            printf("Received packet number %d\n", rspd_pkt.seq_no);
            if (rspd_pkt.type == 3)
                break;

            fwrite(rspd_pkt.data, 1, rspd_pkt.length, resource);
            req_pkt.seq_no = rspd_pkt.seq_no;

            if (sendto(socketfd, &req_pkt, req_pkt.length, 0, (struct sockaddr*) &serveraddr, serverlen) < 0)
                error("ERROR acking");

            printf("ACK'd packet %d\n", req_pkt.seq_no);
            expected_seq_no++;
        }
    }
    fclose(resource);
    return 0;
}
