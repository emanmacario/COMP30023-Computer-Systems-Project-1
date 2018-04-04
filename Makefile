# Makefile for COMP30023 Project 1
# Made by Emmanuel Macario

CC     = gcc
CFLAGS = -Wall
OBJ    = server.o
EXE    = server

# Main program
$(EXE): $(OBJ)
	$(CC) $(CFLAGS) -o $(EXE) $(OBJ)


# Remove executables and object files
clean:
	rm -f $(OBJ) $(EXE)