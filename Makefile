# This Makefile is used to compile the two parts of the project
.PHONY: all clean

# Compile the two parts of the project.
all:
	$(MAKE) -C partA
	$(MAKE) -C partB

# Remove all the object files, shared libraries and executables.
clean:
	$(MAKE) -C partA clean
	$(MAKE) -C partB clean