#include <sys/socket.h>
#include <sys/stat.h>
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
#include "packet.c"

int BUF_SIZE = 1024;
int WIN_SIZE = 5;
char *response_msg = "Request received";

void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[]) {
    int socketfd = 0;
    struct sockaddr_in serv_addr, cli_addr; 
    int pid, clilen, portno, n, base, next_seq_num;
    struct packet req_pkt;
    FILE *resource;

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    portno = atoi(argv[1]);

    /* creates unnamed socket inside the kernel and returns socket descriptor
    * AF_INET specifies IPv4 addresses
    * SOCK_STREAM specifies transport layer should be reliable
    * third argument is left 0 to default to TCP
    */
    socketfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketfd < 0)
        error("ERROR opening socket");
    memset(&serv_addr, '0', sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);

    // assigns the details specified in the structure ‘serv_addr’ to the socket created in the step above. The details include, the family/domain, the interface to listen on(in case the system has multiple interfaces to network) and the port on which the server will wait for the client requests to come.
    if (bind(socketfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0)
        error("ERROR on binding");

    clilen = sizeof(cli_addr);

    int bytes_received;
    // run in an infinite loop so that the server is always running
    while(1) {
        bytes_received = recvfrom(socketfd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen);
        if (bytes_received < 0)
            error("ERROR on receiving from client");
        printf("Server received %d bytes from %s: %s\n", req_pkt.length, inet_ntoa(cli_addr.sin_addr), req_pkt.data);
        base = 1;
        next_seq_num = 1;

        int i, total_packets;
        resource = fopen(req_pkt.data, "rb");
        if (resource == NULL)
            response_msg = "ERROR opening file";

        printf("%s\n", req_pkt.data);
        struct packet rspd_pkt;
        struct stat st;
        stat(req_pkt.data, &st);
        bzero((char *) &rspd_pkt, sizeof(rspd_pkt));
        rspd_pkt.type = 2;
        total_packets = st.st_size / DATA_SIZE;
        printf("TOTAL %d\n", total_packets);

        for (i = 1; i <= WIN_SIZE; i++) {
            rspd_pkt.seq_no = i;
            n = fread(rspd_pkt.data, 1, DATA_SIZE, resource);
            printf("LENGTH %d\n", n);
            rspd_pkt.length = n;
            if(sendto(socketfd, &rspd_pkt, sizeof(int) * 3 + rspd_pkt.length, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
                error("ERROR on sending");
            printf("Sent packet number %d\n", rspd_pkt.seq_no);
            next_seq_num++;
        }

        while(base <= total_packets) {
            if (recvfrom(socketfd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen) > 0) {
                printf("Received ACK for packet %d\n", req_pkt.seq_no);
                base = req_pkt.seq_no + 1;

                if (next_seq_num < total_packets) {
                    rspd_pkt.seq_no = next_seq_num;
                    n = fread(rspd_pkt.data, 1, DATA_SIZE, resource);
                    rspd_pkt.length = n;
                    if(sendto(socketfd, &rspd_pkt, sizeof(int) * 3 + rspd_pkt.length, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
                        error("ERROR on sending");
                    printf("Sent packet number %d\n", next_seq_num);
                    next_seq_num++;
                }
            }
        }
        bzero((char *) &rspd_pkt, sizeof(rspd_pkt));
        rspd_pkt.type = 3;
        puts("Teardown");
        if(sendto(socketfd, &rspd_pkt, sizeof(rspd_pkt.type) * 3, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
            error("ERROR on sending");
        fclose(resource);
    }
}