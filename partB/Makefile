# Use the gcc compiler.
CC = gcc

# Flags for the compiler. Can also use -g for debugging.
CFLAGS = -Wall -Wextra

# Header files.
HEADERS = traceroute.h

# Default target - compile everything and create the executables and libraries.
all: traceroute

# Alias for the default target.
default: all

# Compile the traceroute program
traceroute: traceroute.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $<

# Clean up generated files
clean:
	rm -f traceroute