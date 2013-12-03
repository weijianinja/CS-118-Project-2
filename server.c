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

#define WIN_SIZE 5
#define TIMEOUT 5000

struct packet WINDOW[WIN_SIZE];

void error(char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int min(int a, int b) {
    return a < b ? a : b;
}

void clear_window() {
    bzero((char *) WINDOW, sizeof(WINDOW));
}

int ack_packet(int seq_no) {
    int i, j;
    for(i = 0; i < WIN_SIZE; i++) {
        if (seq_no < WINDOW[i].seq_no)
            return 1; // duplicate ACK

        if (WINDOW[i].seq_no == seq_no) {
            for (j = i; j < WIN_SIZE - 1; j++) {
                WINDOW[j] = WINDOW[j + 1];
            }
            return 0;
        }
    }
    error("Packet isn't in the window");
    return 2;
}

void add_packet(struct packet new) {
    int i;
    for (i = 1; i < WIN_SIZE; i++) {
        if (WINDOW[i].seq_no == WINDOW[i - 1].seq_no) {
            WINDOW[i] = new;
            return;
        }
    }
    error("Window is full!");
}

struct packet get_packet(int seq_no) {
    int i;
    for (i = 0; i < WIN_SIZE; i++) {
        if (WINDOW[i].seq_no == seq_no) {
            return WINDOW[i];
        }
    }
    error("Packet isn't in the window");
}

int main(int argc, char *argv[]) {
    int socketfd = 0, mode = 0;
    struct sockaddr_in serv_addr, cli_addr; 
    int pid, clilen, portno, n, base, next_seq_num;
    struct packet req_pkt, rspd_pkt;
    struct stat st;
    FILE *resource;
    time_t timer;

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

    // run in an infinite loop so that the server is always running
    while(1) {        
        if (recvfrom(socketfd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen) < 0)
            error("ERROR on receiving from client");
        printf("Server received %d bytes from %s: %s\n", req_pkt.length, inet_ntoa(cli_addr.sin_addr), req_pkt.data);

        base = 1;
        next_seq_num = 1;
        clear_window();

        int i, total_packets;
        resource = fopen(req_pkt.data, "rb");
        if (resource == NULL)
            error("ERROR opening file");

        stat(req_pkt.data, &st);
        total_packets = st.st_size / DATA_SIZE;
        printf("Total packets: %d\n", total_packets);

        bzero((char *) &rspd_pkt, sizeof(rspd_pkt));
        rspd_pkt.type = 2;

        for (i = 0; i < WIN_SIZE; i++) {
            rspd_pkt.seq_no = i + 1;
            rspd_pkt.length = fread(rspd_pkt.data, 1, DATA_SIZE, resource);
            WINDOW[i] = rspd_pkt;
            if (sendto(socketfd, &rspd_pkt, sizeof(int) * 3 + rspd_pkt.length, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
                error("ERROR on sending");
            printf("Sent packet number %d\n", rspd_pkt.seq_no);
            next_seq_num++;
        }

        while(base <= total_packets) {
            if (mode == 1 && time(NULL) > timer + TIMEOUT) {
                for (i = 0; i < WIN_SIZE; i++) {
                    if (WINDOW[i].seq_no >= next_seq_num || WINDOW[i].seq_no == WINDOW[i - 1].seq_no)
                        break;
                    else if (sendto(socketfd, &WINDOW[i], sizeof(int) * 3 + WINDOW[i].length, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
                        error("ERROR on sending");
                }
                mode = 0;
            }
            if (recvfrom(socketfd, &req_pkt, sizeof(req_pkt), 0, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen) > 0) {
                printf("Received ACK for packet %d\n", req_pkt.seq_no);
                if (ack_packet(req_pkt.seq_no) == 1) {
                    mode = 1;
                    time(&timer);
                    continue;
                }
                base = req_pkt.seq_no + 1;

                if (next_seq_num <= min(base + WIN_SIZE, total_packets)) {
                    rspd_pkt.seq_no = next_seq_num;
                    rspd_pkt.length = fread(rspd_pkt.data, 1, DATA_SIZE, resource);
                    add_packet(rspd_pkt);
                    if (sendto(socketfd, &rspd_pkt, sizeof(int) * 3 + rspd_pkt.length, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
                        error("ERROR on sending");
                    printf("Sent packet number %d\n", next_seq_num);
                    next_seq_num++;
                }
            }
        }
        bzero((char *) &rspd_pkt, sizeof(rspd_pkt));
        rspd_pkt.type = 3;
        puts("Teardown");
        if (sendto(socketfd, &rspd_pkt, sizeof(rspd_pkt.type) * 3, 0, (struct sockaddr *) &cli_addr, clilen) < 0)
            error("ERROR on sending");
        fclose(resource);
    }
    return 0;
}
