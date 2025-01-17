#ifndef _TRACEROUTE_H
#define _TRACEROUTE_H

#define PACKET_SIZE 64
#define MAX_HOPS 30
#define TRIES_PER_HOP 3
#define TIMEOUT 1 // seconds

// Function declarations
unsigned short calculate_checksum(unsigned short *addr, int len);
double get_time_ms(void);
int send_probe(int sockfd, struct sockaddr_in *dest_addr, int seq);
void print_probe_results(int ttl, struct sockaddr_in *recv_addr, int replies, double times[]);

#endif // _TRACEROUTE_H