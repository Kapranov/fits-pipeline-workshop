# Compiler settings
CC = gcc
FC = gfortran
CFLAGS = -Wall -O2
FFLAGS = -Wall -O2

# Libraries
LIBS = -lcfitsio

# Target executable names
TARGET = read_image
OBJS = bridge.o read_image.o

# Default target
all: $(TARGET)

# Link the final executable
$(TARGET): $(OBJS)
	$(FC) $(FFLAGS) $(OBJS) -o $(TARGET) $(LIBS)

# Compile C source
bridge.o: bridge.c
	$(CC) $(CFLAGS) -c bridge.c -o bridge.o

# Compile Fortran source
read_image.o: read_image.f90
	$(FC) $(FFLAGS) -c read_image.f90 -o read_image.o

# Clean up build artifacts
clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
