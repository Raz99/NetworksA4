#include <stdio.h>           // Standard input/output definitions
#include <arpa/inet.h>       // Definitions for internet operations (inet_pton, inet_ntoa)
#include <netinet/in.h>      // Internet address family (AF_INET, AF_INET6)
#include <netinet/ip.h>      // Definitions for internet protocol operations (IP header)
#include <netinet/ip6.h>     // Definitions for IPv6 header
#include <netinet/ip_icmp.h> // Definitions for internet control message protocol operations (ICMP header)
#include <netinet/icmp6.h>   // Added for IPv6
#include <poll.h>            // Poll API for monitoring file descriptors (poll)
#include <errno.h>           // Error number definitions. Used for error handling (EACCES, EPERM)
#include <string.h>          // String manipulation functions (strlen, memset, memcpy)
#include <sys/socket.h>      // Definitions for socket operations (socket, sendto, recvfrom)
#include <sys/time.h>        // Time types (struct timeval and gettimeofday)
#include <unistd.h>          // UNIX standard function definitions (getpid, close, sleep)
#include <getopt.h>          // Parser
#include <stdlib.h>          // For atoi()
#include <signal.h>          // Signal handling
#include "ping.h"            // Header file for the program (calculate_checksum function and some constants)

// Structure to hold ping options
struct ping_options
{
    char *address;
    int type;
    int count;
    int flood;
};

// Structure to hold ping statistics
struct ping_stats
{
    int transmitted;
    int received;
    double min_rtt;
    double max_rtt;
    double total_rtt;
    struct timeval start_time;
};

// Global variables 
int keep_running = 1; // Flag to keep the main loop running

struct ping_options options = {
    .address = NULL,
    .type = 0,
    .count = -1,
    .flood = 0
    };

struct ping_stats stats = {
    .transmitted = 0,
    .received = 0,
    .min_rtt = 999999,
    .max_rtt = 0,
    .total_rtt = 0,
    .start_time = {0, 0}
    };

// Signal handler function to display statistics and exit
void display_statistics(int signum)
{
    (void)signum; // Explicitly mark parameter as unused
    struct timeval end_time;
    gettimeofday(&end_time, NULL);
    double total_time = (end_time.tv_sec - stats.start_time.tv_sec) * 1000.0 +
                        (end_time.tv_usec - stats.start_time.tv_usec) / 1000.0;

    printf("\n--- %s ping statistics ---\n", options.address);
    printf("%d packets transmitted, %d received, time %.1fms\n",
           stats.transmitted,
           stats.received,
           total_time);

    if (stats.received > 0)
    {
        double avg_rtt = stats.total_rtt / stats.received;
        printf("rtt min/avg/max = %.3f/%.3f/%.3fms\n",
               stats.min_rtt, avg_rtt, stats.max_rtt);
    }

    keep_running = 0; // Stop the main loop
}

/**
 * Creates and configures a raw socket for sending ICMP or ICMPv6 packets.
 * @param ip_type The IP type (4 for IPv4, 6 for IPv6).
 * @param input_addr The destination IP address as a string.
 * @param dest_addr4 Pointer to a sockaddr_in structure for IPv4.
 * @param dest_addr6 Pointer to a sockaddr_in6 structure for IPv6.
 * @return The socket file descriptor on success, or 1 on error.
 */
int create_socket(int ip_type, char *input_addr, void *dest_addr4, void *dest_addr6)
{
    int sock_fd;

    if (ip_type == 4)
    {
        struct sockaddr_in *dest_addr_v4 = (struct sockaddr_in *)dest_addr4;
        memset(dest_addr_v4, 0, sizeof(struct sockaddr_in));
        dest_addr_v4->sin_family = AF_INET;

        if (inet_pton(AF_INET, input_addr, &dest_addr_v4->sin_addr) <= 0)
        {
            fprintf(stderr, "Error: \"%s\" is not a valid IPv4 address\n", input_addr);
            return 1;
        }

        sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    }

    else if (ip_type == 6)
    {
        struct sockaddr_in6 *dest_addr_v6 = (struct sockaddr_in6 *)dest_addr6;
        memset(dest_addr_v6, 0, sizeof(struct sockaddr_in6));
        dest_addr_v6->sin6_family = AF_INET6;

        if (inet_pton(AF_INET6, input_addr, &dest_addr_v6->sin6_addr) <= 0)
        {
            fprintf(stderr, "Error: \"%s\" is not a valid IPv6 address\n", input_addr);
            return 1;
        }

        sock_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
    }

    else
    {
        fprintf(stderr, "Invalid IP type. Must be 4 or 6\n");
        return 1;
    }

    if (sock_fd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    return sock_fd;
}

/**
 * Parses arguments into a ping_options structure.
 * @param argc The argument count.
 * @param argv The argument array.
 * @param options Pointer to a ping_options structure to stordisplaye parsed options.
 * @return 0 on success, or 1 on error.
 */
int parse_arguments(int argc, char *argv[], struct ping_options *options)
{
    int opt;
    int a_flag = 0, t_flag = 0;

    while ((opt = getopt(argc, argv, "a:t:c:f")) != -1)
    {
        switch (opt)
        {
        case 'a':
            options->address = optarg; // Store the address argument
            a_flag = 1;                // Indicate that the address flag is set
            break;
        case 't':
            options->type = atoi(optarg); // Convert type argument to integer
            if (options->type != 4 && options->type != 6)
            {
                fprintf(stderr, "Type must be either 4 or 6\n");
                return 1;
            }
            t_flag = 1; // Indicate that the type flag is set
            break;
        case 'c':
            options->count = atoi(optarg); // Convert count argument to integer
            if (options->count <= 0)
            {
                fprintf(stderr, "Count must be positive\n");
                return 1;
            }
            break;
        case 'f':
            options->flood = 1; // Set flood flag if -f is specified
            break;
        default:
            fprintf(stderr, "Usage: %s -a <address> -t <4|6> [-c count] [-f]\n", argv[0]);
            return 1;
        }
    }

    if (!a_flag || !t_flag)
    {
        fprintf(stderr, "Both -a and -t flags are required\n");
        return 1;
    }
    return 0;
}

// Signal handler for SIGINT
void handle_sigint(int signum)
{
    (void)signum; // Explicitly mark parameter as unused
    keep_running = 0;
    display_statistics(signum);
}

int main(int argc, char *argv[])
{
    if (parse_arguments(argc, argv, &options) != 0)
    {
        return 1;
    }

    // Set up signal handler for SIGINT (Ctrl+C)
    signal(SIGINT, handle_sigint);

    // Structure to store the destination address.
    // Even though we are using raw sockets, creating from zero the IP header is a bit complex,
    // we use the structure to store the destination address.
    struct sockaddr_in dest_addr_v4;
    struct sockaddr_in6 dest_addr_v6;

    // Create a raw socket with the ICMP protocol.
    int sock = create_socket(options.type, options.address, &dest_addr_v4, &dest_addr_v6);

    // Error handling if the socket creation fails (could happen if the program isn't run with sudo).
    if (sock < 0)
    {
        perror("socket(2)");

        // Check if the error is due to permissions and print a message to the user.
        // Some magic constants for the error numbers, which are defined in the errno.h header file.
        if (errno == EACCES || errno == EPERM)
            fprintf(stderr, "You need to run the program with sudo.\n");

        return 1;
    }

    gettimeofday(&stats.start_time, NULL); // Record the start time

    // Just some buffer to store the ICMP packet itself. We zero it out to make sure there are no garbage values.
    char buffer[BUFFER_SIZE];

    // The payload of the ICMP packet. Can be anything, as long as it's a valid string.
    // We use some garbage characters, as well as some ASCII characters, to test the program.
    char *msg = "ABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890!@#$^&*()_+{}|:<>?~`-=[]',.";

    // Payload size of the ICMP packet.
    // We need to add 1 to the size of the payload, as we need to include the null-terminator of the string.
    int payload_size = strlen(msg) + 1;

    // The sequence number of the ping request.
    // It starts at 0 and is incremented by 1 for each new request.
    // Good for identifying the order of the requests.
    int seq = 0;

    // Create a pollfd structure to wait for the socket to become ready for reading.
    // Used for receiving the ICMP reply packet, as it may take some time to arrive or not arrive at all.
    struct pollfd fds[1];

    // Set the file descriptor of the socket to the pollfd structure.
    fds[0].fd = sock;

    // Set the events to wait for to POLLIN, which means the socket is ready for reading.
    fds[0].events = POLLIN;

    fprintf(stdout, "Pinging %s with %d bytes of data:\n", options.address, payload_size);

    // The main loop of the program.
    while (keep_running && (options.count == -1 || stats.transmitted < options.count))
    {
        // Zero out the buffer to make sure there are no garbage values.
        memset(buffer, 0, sizeof(buffer)); // Clear the buffer

        if (options.type == 4)
        {
            struct icmphdr *icmp_header = (struct icmphdr *)buffer;
            icmp_header->type = ICMP_ECHO;                                                             // Set the type of the ICMP packet to ECHO REQUEST (PING).
            icmp_header->code = 0;                                                                     // Set the code of the ICMP packet to 0 (As it isn't used in the ECHO type).
            icmp_header->un.echo.id = htons(getpid());                                                 // Set the ICMP identifier.
            icmp_header->un.echo.sequence = htons(seq);                                                // Set the sequence number.
            memcpy(buffer + sizeof(struct icmphdr), msg, payload_size);                                // Copy the payload to the buffer.
            icmp_header->checksum = 0;                                                                 // Set the checksum of the ICMP packet to 0, as we need to calculate it.
            icmp_header->checksum = calculate_checksum(buffer, sizeof(struct icmphdr) + payload_size); // Calculate the checksum of the ICMP packet.
        }

        else if (options.type == 6)
        {
            struct icmp6_hdr *icmp6_header = (struct icmp6_hdr *)buffer;
            icmp6_header->icmp6_type = ICMP6_ECHO_REQUEST;                // Set the type of the ICMP packet to ICMP6 ECHO REQUEST (PING).
            icmp6_header->icmp6_code = 0;                                 // Set the code of the ICMP packet to 0 (As it isn't used in the ECHO type).
            icmp6_header->icmp6_id = htons(getpid());                     // Set the ICMP identifier.
            icmp6_header->icmp6_seq = htons(seq);                         // Set the sequence number.
            memcpy(buffer + sizeof(struct icmp6_hdr), msg, payload_size); // Copy the payload to the buffer.
            icmp6_header->icmp6_cksum = 0;                                // Set the checksum of the ICMP packet to 0, as we need to calculate it.
        }

        // Calculate the time it takes to send and receive the packet.
        struct timeval start, end;
        gettimeofday(&start, NULL);

        int bytes_sent; // Number of bytes sent

        // Send the ICMP packet
        if (options.type == 4)
        {
            bytes_sent = sendto(sock, buffer, sizeof(struct icmphdr) + payload_size, 0,
                                (struct sockaddr *)&dest_addr_v4, sizeof(dest_addr_v4));
        }

        else if (options.type == 6)
        {
            bytes_sent = sendto(sock, buffer, sizeof(struct icmp6_hdr) + payload_size, 0,
                                (struct sockaddr *)&dest_addr_v6, sizeof(dest_addr_v6));
        }

        // If the packet sending fails, print an error message and close the socket
        if (bytes_sent <= 0)
        {
            perror("sendto(2)");
            close(sock);
            return 1;
        }

        stats.transmitted++; // Increment the transmitted counter

        int ret = poll(fds, 1, TIMEOUT); // Poll the socket to wait for the ICMP reply packet

        // The poll(2) function returns 0 if the socket is not ready for reading after the timeout.
        if (ret == 0)
        {
            fprintf(stderr, "Request timeout for icmp_seq %d\n", seq + 1);
            continue;
        }

        // The poll(2) function returns a negative value if an error occurs.
        else if (ret < 0)
        {
            perror("poll(2)");
            close(sock);
            return 1;
        }

        if (fds[0].revents & POLLIN)
        { // Check if the socket is ready for reading
            if (options.type == 4)
            {
                struct sockaddr_in source_addr; // Temporary structure to store the source address of the ICMP reply packet.

                int bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0,
                                              (struct sockaddr *)&source_addr, &(socklen_t){sizeof(source_addr)});
                if (bytes_received <= 0)
                {
                    perror("recvfrom(2)");
                    close(sock);
                    return 1;
                }

                gettimeofday(&end, NULL); // Record the receive time
                struct iphdr *ip_header = (struct iphdr *)buffer;
                struct icmphdr *icmp_reply = (struct icmphdr *)(buffer + ip_header->ihl * 4);

                if (icmp_reply->type == ICMP_ECHOREPLY)
                {
                    double rtt = (end.tv_sec - start.tv_sec) * 1000.0 +
                                 (end.tv_usec - start.tv_usec) / 1000.0; // Calculate round-trip time

                    stats.received++;                                            // Increment the received counter
                    stats.min_rtt = (rtt < stats.min_rtt) ? rtt : stats.min_rtt; // Update minimum RTT
                    stats.max_rtt = (rtt > stats.max_rtt) ? rtt : stats.max_rtt; // Update maximum RTT
                    stats.total_rtt += rtt;                                      // Update total RTT

                    // Print the result of the ping request
                    fprintf(stdout, "%d bytes from %s: icmp_seq=%d ttl=%d time=%.2fms\n",
                            bytes_received - ip_header->ihl * 4, // Print the size of the ICMP reply packet
                            inet_ntoa(source_addr.sin_addr),     // Print source IP address
                            seq + 1,                             // Print sequence number
                            ip_header->ttl,                      // Print TTL
                            rtt);                                // Print round-trip time
                    seq++;
                }
            }

            else if (options.type == 6)
            {
                struct sockaddr_in6 source_addr; // Tempor                               ary structure to store the source address of the ICMPv6 reply packet.
                char addr_str[46];               // Buffer to store the source address as a string

                // Receive the ICMPv6 reply packet
                int bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0,
                                              (struct sockaddr *)&source_addr, &(socklen_t){sizeof(source_addr)});
                if (bytes_received <= 0)
                {
                    perror("recvfrom(2)");
                    close(sock);
                    return 1;
                }

                gettimeofday(&end, NULL); // Record the receive time
                struct icmp6_hdr *icmp6_reply = (struct icmp6_hdr *)buffer;

                if (icmp6_reply->icmp6_type == ICMP6_ECHO_REPLY)
                {
                    double rtt = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0; // Calculate round-trip time

                    stats.received++;                                            // Increment the received counter
                    stats.min_rtt = (rtt < stats.min_rtt) ? rtt : stats.min_rtt; // Update minimum RTT
                    stats.max_rtt = (rtt > stats.max_rtt) ? rtt : stats.max_rtt; // Update maximum RTT
                    stats.total_rtt += rtt;                                      // Update total RTT

                    inet_ntop(AF_INET6, &source_addr.sin6_addr, addr_str, 46); // Convert source address to string

                    // Print the result of the ping request
                    fprintf(stdout, "%d bytes from %s: icmp_seq=%d ttl=%d time=%.2fms\n",
                            bytes_received,
                            addr_str, // Print source address
                            seq + 1,  // Print sequence number
                            64,       // Print TTL
                            rtt);     // Print round-trip time
                    seq++;
                }
            }
        }

        // Sleep for 1 second before sending the next request, if not in flood mode
        if (!options.flood)
        {
            sleep(SLEEP_TIME);
        }
    }

    if(keep_running) {
        display_statistics(0); // Display statistics
    }

    // Close the socket and return 0 to the operating system.
    close(sock);
    return 0;
}

/**
 * Calculates the checksum for the given data.
 * @param data Pointer to the data for which the checksum is to be calculated.
 * @param bytes The number of bytes in the data.
 * @return The calculated checksum as an unsigned short integer.
 */
unsigned short int calculate_checksum(void *data, unsigned int bytes)
{
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;

    // Main summing loop.
    while (bytes > 1)
    {
        total_sum += *data_pointer++; // Some magic pointer arithmetic.
        bytes -= 2;
    }

    // Add left-over byte, if any.
    if (bytes > 0)
        total_sum += *((unsigned char *)data_pointer);

    // Fold 32-bit sum to 16 bits.
    while (total_sum >> 16)
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);

    // Return the one's complement of the result.
    return (~((unsigned short int)total_sum));
}