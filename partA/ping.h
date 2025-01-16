#ifndef _PING_H
#define _PING_H

#define TIMEOUT 10000  // 10 seconds timeout
#define BUFFER_SIZE 1024
#define SLEEP_TIME 1 // seconds

// Function prototype
unsigned short int calculate_checksum(void *data, unsigned int bytes);

#endif // _PING_H