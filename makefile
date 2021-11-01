CC=gcc
INC_DIR = .
CFLAGS=-I$(INC_DIR)

all: server/server client/client
clean:
	rm -f *.o
	rm -f ./server/server ./server/*.o
	rm -f ./client/client ./client/*.o