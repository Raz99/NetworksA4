#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <errno.h>
#include "traceroute.h"

/**
 * Calculates Checksum for IP/ICMP headers
 * @param addr Pointer to the buffer containing data
 * @param len Length of data in bytes
 * @return Checksum value
 */
unsigned short calculate_checksum(unsigned short *addr, int len) {
    unsigned int sum = 0; // Sum to handle overflow
    unsigned short answer = 0; // Final checksum result
    unsigned short *w = addr; // Working pointer
    int nleft = len; // Remaining bytes to process

    // Process at a time
    while (nleft > 1) {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1) {
        *(unsigned char *)(&answer) = *(unsigned char *)w;
        sum += answer;
    }

    // Fold 32-bit sum into 16 bits
    sum = (sum >> 16) + (sum & 0xFFFF); // Add high 16 to low 16
    sum += (sum >> 16); // Add carry
    answer = ~sum; // One's complement
    return answer;
}

/**
 * Gets current time in milliseconds
 * @return Current time in milliseconds as a double
 */
double get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

int send_probe(int sockfd, struct sockaddr_in *dest_addr, int seq) {
    char packet[PACKET_SIZE]; // Packet buffer
    struct icmphdr *icmp_header = (struct icmphdr *)packet; // ICMP header

    // Prepare ICMP packet
    memset(packet, 0, PACKET_SIZE); // Clear packet buffer
    icmp_header->type = ICMP_ECHO; // ICMP Echo Request
    icmp_header->code = 0; // Set the code of the ICMP packet to 0 (As it isn't used in the ECHO type)
    icmp_header->un.echo.sequence = seq; // Set the sequence number
    icmp_header->un.echo.id = getpid(); // Identity
    icmp_header->checksum = 0; // Clear checksum
    icmp_header->checksum = calculate_checksum((unsigned short *)icmp_header, PACKET_SIZE); // Calculate checksum

    return sendto(sockfd, packet, PACKET_SIZE, 0, (struct sockaddr *)dest_addr, sizeof(*dest_addr));
}

void print_probe_results(int ttl, struct sockaddr_in *recv_addr, int replies, double times[]) {
    printf("%2d  ", ttl); // Print TTL

    // Print IP address and RTT times
    if (replies > 0) {
        char ip_str[16]; // IP address string (up to 3 digits for each cell and there are 4 cells + 3 dots + "\0")
        inet_ntop(AF_INET, &recv_addr->sin_addr, ip_str, sizeof(ip_str)); // Convert IP address to string
        printf("%s  ", ip_str); // Print IP address

        for (int i = 0; i < TRIES_PER_HOP; i++) {
            if (i < replies) {
                printf("%.3fms", times[i]); // Print RTT time
            }

            else {
                printf("*"); // Print "*" for missing reply
            }

            if (i < TRIES_PER_HOP - 1) {
                printf("  "); // Print space between times
            }
        }
    }
    
    // No replies case
    else {
        printf("* * *");
    }
    
    printf("\n");
}

int main(int argc, char *argv[]) {
    // Check arguments and usage
    if (argc != 3 || strcmp(argv[1], "-a") != 0) {
        printf("Invalid arguments.\n");
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP); // Create raw socket (related to IPv4)

    // Check socket creation
    if (sockfd < 0) {
        perror("Socket creation failed");
        return 1;
    }

    // Set socket timeout
    struct timeval tv = {TIMEOUT, 0}; 
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Prepare destination address
    struct sockaddr_in dest_addr; // Destination address
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET; // IPv4
    if (inet_pton(AF_INET, argv[2], &dest_addr.sin_addr) <= 0) { // Convert IP address to binary
        printf("Invalid address\n");
        close(sockfd);
        return 1;
    }

    printf("traceroute to %s, %d hops max\n", argv[2], MAX_HOPS);

    char recv_packet[PACKET_SIZE];
    int seq = 1; // Sequence number for each probe

    // Main loop for each TTL
    for (int ttl = 1; ttl <= MAX_HOPS; ttl++) {
        setsockopt(sockfd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)); // Set TTL

        struct sockaddr_in recv_addr; // Address of the received packet
        socklen_t addr_len = sizeof(recv_addr); // Length of the address
        int reached_dest = 0; // Flag to check if destination reached
        int replies = 0; // Number of replies
        double times[TRIES_PER_HOP]; // Array for round-trip times

        // Send probes for each TTL
        for (int try = 0; try < TRIES_PER_HOP; try++) { 
            double send_time = get_time_ms(); // Time of sending probe
            
            // Send probe and check for errors
            if (send_probe(sockfd, &dest_addr, seq++) <= 0) {
                perror("sendto failed");
                continue;
            }

            memset(recv_packet, 0, PACKET_SIZE); // Clear receive buffer
            int recv_len = recvfrom(sockfd, recv_packet, PACKET_SIZE, 0, (struct sockaddr *)&recv_addr, &addr_len); // Receive packet

            if (recv_len > 0) { // Check if packet received
                times[replies] = get_time_ms() - send_time; // Calculate round-trip time
                replies++;

                if (recv_addr.sin_addr.s_addr == dest_addr.sin_addr.s_addr) { // Check if destination reached
                    reached_dest = 1; // Set flag
                }
            }
        }

        print_probe_results(ttl, &recv_addr, replies, times); // Print probe results

        if (reached_dest) { // Check if destination reached
            break; // Exit loop
        }
    }

    // Close the socket and return 0 to the operating system
    close(sockfd);
    return 0;
}