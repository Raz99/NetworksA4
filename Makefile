# Compiler and compiler flags
CC = gcc
CFLAGS = -Wall -Wextra -O2

# Programs to build
PROGRAMS = ping traceroute

# Default IP address to ping and traceroute
IP = 8.8.8.8

# Default IP type to ping
PING_IP_TYPE = 4

# Default target
all: $(PROGRAMS)

# Alias for the default target
default: all

# Compile the ping program
ping: ping.o
	$(CC) $(CFLAGS) -o $@ $^

# Compile the traceroute program
traceroute: traceroute.o
	$(CC) $(CFLAGS) -o $@ $^

# Run the ping program in sudo mode
runp: ping
	sudo ./ping -a $(IP) -t $(PING_IP_TYPE)

# Run the traceroute program in sudo mode
runt: traceroute
	sudo ./traceroute -a $(IP)

# Object files of ping
ping.o: ping.c ping.h
	$(CC) $(CFLAGS) -c ping.c

# Object files of traceroute
traceroute.o: traceroute.c traceroute.h
	$(CC) $(CFLAGS) -c traceroute.c

# Clean up
clean:
	rm -f *.o ping traceroute