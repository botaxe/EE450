# Build executables named client, ServerD, ServerS, ServerU, and serverM
CC = g++
CFLAGS = -Wall -Wextra

all: client ServerD ServerS ServerU serverM

client: client.cpp
	$(CC) $(CFLAGS) -o client client.cpp

ServerD: ServerD.cpp
	$(CC) $(CFLAGS) -o ServerD ServerD.cpp

ServerS: ServerS.cpp
	$(CC) $(CFLAGS) -o ServerS ServerS.cpp

ServerU: ServerU.cpp
	$(CC) $(CFLAGS) -o ServerU ServerU.cpp

serverM: serverM.cpp
	$(CC) $(CFLAGS) -o serverM serverM.cpp

clean:
	$(RM) client ServerD ServerS ServerU serverM
