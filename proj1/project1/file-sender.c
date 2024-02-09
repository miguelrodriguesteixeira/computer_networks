#include "packet-format.h"
#include <limits.h>
#include <netdb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>

#define TIMEOUT_SEC 1 // Timeout in secs

int main(int argc, char *argv[]) {
    // Command line arguments: file name, host, port, and window size
    char *file_name = argv[1];
    char *host = argv[2];
    int port = atoi(argv[3]);
    int window_size = atoi(argv[4]);

    // Open the file for reading
    FILE *file = fopen(file_name, "r");
    if (!file) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    // Prepare server host address
    struct hostent *he;
    if (!(he = gethostbyname(host))) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    // Prepare server address structure
    struct sockaddr_in srv_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = *((struct in_addr *)he->h_addr),
    };

    // Create a UDP socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    // Initialize sequence number, data packet, and acknowledgment packet
    uint32_t seq_num = 0;
    data_pkt_t data_pkt;
    size_t data_len;
    ack_pkt_t ack_pkt;

    // Set receive timeout using setsockopt
    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    do {
        // Generate segments from file until the end of the file
            // Prepare data segment
            data_pkt.seq_num = htonl(seq_num++);

            // Load data from file
            data_len = fread(data_pkt.data, 1, sizeof(data_pkt.data), file);

            while (true) {
                // Send segment
                ssize_t sent_len = sendto(sockfd, &data_pkt, offsetof(data_pkt_t, data) + data_len, 0,
                                          (struct sockaddr *)&srv_addr, sizeof(srv_addr));
                printf("Sending segment %d, size %ld.\n", ntohl(data_pkt.seq_num),
                       offsetof(data_pkt_t, data) + data_len);

                // Check for packet truncation
                if (sent_len != (ssize_t)(offsetof(data_pkt_t, data) + data_len)) {
                    fprintf(stderr, "Truncated packet.\n");
                    exit(EXIT_FAILURE);
                }

                // Receive response from the receiver
                ssize_t recv_len = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0,
                                            (struct sockaddr *)&srv_addr, &(socklen_t){sizeof(srv_addr)});
                if (recv_len == -1) {
                    // Check if the error is due to a timeout
                    if (errno == EWOULDBLOCK || errno == EAGAIN) {
                        printf("Timeout occurred. Retrying...\n");

                    } else {
                        perror("recvfrom");
                        exit(EXIT_FAILURE);
                    }
                } else {
                    break; // Exit the retry loop upon successful reception
                }

            // Print received response
            printf("Received response from receiver: %s\n", data_pkt.data);
        }
    } while (!(feof(file) && data_len < sizeof(data_pkt.data)));

    // Clean up and exit
    close(sockfd);
    fclose(file);

    return EXIT_SUCCESS;
}
