#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <getopt.h>

void print_usage() {
    printf("Usage: sudo ./discovery -a <IP> -c <subnet-mask>\n");
}

int is_ip_active(char *ip) {
    int sock;
    struct sockaddr_in server;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 0;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(80); // HTTP port
    inet_pton(AF_INET, ip, &server.sin_addr);

    int result = connect(sock, (struct sockaddr *)&server, sizeof(server));
    close(sock);

    return (result == 0);
}

void scan_range(char *base_ip, int subnet_mask) {
    struct in_addr addr;
    inet_pton(AF_INET, base_ip, &addr);
    int host_count = (1 << (32 - subnet_mask)) - 2; // Number of hosts in the subnet

    printf("scanning %s/%d:\n", base_ip, subnet_mask);

    for (int i = 1; i <= host_count; i++) {
        addr.s_addr = htonl(ntohl(addr.s_addr) + 1);
        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr, ip, INET_ADDRSTRLEN);

        if (is_ip_active(ip)) {
            printf("%s\n", ip);
        }
    }

    printf("Scan Complete!\n");
}

int main(int argc, char *argv[]) {
    char *ip = NULL;
    int subnet_mask = 0;
    int opt;

    while ((opt = getopt(argc, argv, "a:c:")) != -1) {
        switch (opt) {
        case 'a':
            ip = optarg;
            break;
        case 'c':
            subnet_mask = atoi(optarg);
            break;
        default:
            print_usage();
            exit(EXIT_FAILURE);
        }
    }

    if (ip == NULL || subnet_mask == 0) {
        print_usage();
        exit(EXIT_FAILURE);
    }

    scan_range(ip, subnet_mask);

    return 0;
}