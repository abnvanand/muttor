# all: library.cpp main.cpp
#
# $@ evaluates to all
# $< evaluates to library.cpp
# $^ evaluates to library.cpp main.cpp

CC = g++
CFLAGS = -Wall -std=c++1z

CL_OBJ =  client.o utils.o
TR_OBJ =  tracker.o utils.o

DEPS = utils.h

# all such targets which do not create a file
.PHONY : all clean

%.o: %.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< -lcrypto -pthread

# rule to build both tracker and client
# TODO: find if this is the correct way to do this
all:
	make tracker
	make client

tracker: $(TR_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lcrypto -pthread

# -lcrypto will include the crypto library from openssl
client: $(CL_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lcrypto -pthread

clean:
	rm -f *.o tracker client