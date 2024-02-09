#include "packet-format.h"
#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

#define TIMEOUT_SEC 4 // Timeout 4 secs

int main(int argc, char *argv[]) {
    // Command line arguments: file name and port number
    char *file_name = argv[1];
    int port = atoi(argv[2]);
    int window_size = atoi(argv[3]);

    // Open the file for writing
    FILE *file = fopen(file_name, "w");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Prepare server socket.
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Allow address reuse so we can rebind to the same port
    // after restarting the server
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    // Set up server address structure
    struct sockaddr_in srv_addr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(INADDR_ANY),
        .sin_port = htons(port),
    };

    // Bind the socket to the server address structure
    if (bind(sockfd, (struct sockaddr *)&srv_addr, sizeof(srv_addr))) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    fprintf(stderr, "Receiving on port: %d\n", port);

    // Set receive timeout using setsockopt
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC; // Set the timeout in seconds
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Variables for receiving data
    ssize_t len;
    int end_of_file = 0;
    // Receive segment and acknowledgment packet
    struct sockaddr_in src_addr;
    data_pkt_t data_pkt;
    ack_pkt_t ack_pkt;

    while (true) {
        // Receive segment
        len = recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0,
                       (struct sockaddr *)&src_addr, &(socklen_t){sizeof(src_addr)});

        // Check if the received packet is smaller than expected,
        // indicating the end of the file or an error condition
        if (len < sizeof(data_pkt)) {
            end_of_file = 1;
        }

        // If a timeout occurs, break the loop and handle it separately
        if (len == -1) {
            break;
        }

        // Print information about the received segment
        printf("Received segment %d, size %ld.\n", ntohl(data_pkt.seq_num), len);

        // Increment the sequence number and prepare an acknowledgment packet
        int seq_num = ntohl(data_pkt.seq_num);
        seq_num++;
        ack_pkt.seq_num = htonl(seq_num);

        // Write data to file.
        fwrite(data_pkt.data, 1, len - offsetof(data_pkt_t, data), file);

        // Send an acknowledgment back to the sender.
        sendto(sockfd, &ack_pkt, sizeof(ack_pkt_t), 0, (struct sockaddr *)&src_addr, sizeof(src_addr));
    }

    // Handle the end of file or timeout condition
    if (end_of_file == 1) {
        // Check if the error is due to a timeout
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            printf("Timeout occurred.\n");
        } 
        else {
            perror("recvfrom");
        }

        // Clean up and exit.
        close(sockfd);
        fclose(file);
        exit(EXIT_FAILURE);
    }

    // Clean up and exit.
    close(sockfd);
    fclose(file);

    return EXIT_SUCCESS;
}
