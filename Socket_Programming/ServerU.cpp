#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <string>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>
#include <sys/wait.h>
#include <netdb.h>

using namespace std;

#define MAXBUFLEN 100

map<string, int> roomInfo; // map to store all of backend database

// Function to parse data from the given txt file and return it as a string
string parseDataFromFile(const string &fileName)
{
    ifstream file(fileName.c_str());
    string parsedData;
    string line;
    while (getline(file, line))
    {
        stringstream ss(line);
        string roomCode;
        string numRoomStr;
        if (getline(ss, roomCode, ',') && getline(ss, numRoomStr))
        {
            parsedData += roomCode + "," + numRoomStr + "\n";
            roomInfo[roomCode] = atoi(numRoomStr.c_str());
        }
        else
        {
            cerr << "Error: Failed to parse line: " << line << endl;
        }
    }
    file.close();
    return parsedData;
}

// Function to send UDP message from Beej broadcaster.c
void sendUDP(const string &data, const string &port)
{
    int udpSock;
    struct sockaddr_in broadcast_addr;
    if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        // perror("socket");
        return;
    }
    int broadcast = 1;
    if (setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0)
    {
        close(udpSock);
        return;
    }
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(atoi(port.c_str()));
    broadcast_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (sendto(udpSock, data.c_str(), data.length(), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) == -1)
    {
        // perror("sendto");
        return;
    }
    close(udpSock);
}

// Function to receive UDP messages on a specific port and return the received message inspired by beej listener.c 
string receiveUDP(const string &port)
{
    int udpSock;
    struct sockaddr_in my_addr, their_addr;
    socklen_t addr_len;
    char buf[MAXBUFLEN];
    if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
    {
        // perror("socket");
        return "";
    }
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(atoi(port.c_str()));
    my_addr.sin_addr.s_addr = INADDR_ANY; // use my IPv4 address
    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));

    if (bind(udpSock, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1)
    {
        // perror("bind");
        close(udpSock);
        return "";
    }
    string receivedMessage;
    while (true)
    {
        addr_len = sizeof(their_addr);
        memset(buf, 0, sizeof(buf));

        if (recvfrom(udpSock, buf, MAXBUFLEN - 1, 0, (struct sockaddr *)&their_addr, &addr_len) == -1)
        {
            // perror("recvfrom");
            continue;
        }
        receivedMessage = buf;
        break; 
    }
    close(udpSock);
    return receivedMessage; 
}

int main()
{
    cout << "The Server U is up and running using UDP on port 43489." << endl;
    string parsedData = parseDataFromFile("./suite.txt");
    sendUDP(parsedData, "44489");
    cout << "The Server U has sent the room status to the main server." << endl;
    while (true)
    {
        string receivedMessage = receiveUDP("43489");
        string action = receivedMessage.substr(0, 1);
        string roomCode = receivedMessage.substr(3);
        if (action == "A")
        {   
            cout << "The Server U received an availability request from the main server." << endl;
            if (roomInfo.find(roomCode) == roomInfo.end())
            {
                cout << "Not able to find the room layout." << endl;
                sendUDP("No room", "44489");
            }
            else
            {
                if (roomInfo[roomCode] > 0)
                {
                    cout << "Room " << roomCode << " is available." << endl;
                    sendUDP("Avail", "44489");
                }
                else
                {
                    cout << "Room " << roomCode << " is not available." << endl;
                    sendUDP("Not avail", "44489");
                }
            }
            cout << "The Server U finished sending the response to the main server." << endl;
        }
        else
        {
            cout << "The Server U received a reservation request from the main server." << endl;
            if (roomInfo.find(roomCode) == roomInfo.end())
            {
                cout << "Cannot make a reservation. Not able to find the room layout." << endl;
                sendUDP("No room", "44489");
                cout << "The Server U finished sending the response to the main server." << endl;
            }
            else
            {
                if (roomInfo[roomCode] > 0)
                {
                    roomInfo[roomCode]--;
                    cout << "Successful reservation. The count of Room " << roomCode << " is now " << roomInfo[roomCode] << "." << endl;
                    sendUDP("Reserve", "44489");
                    cout << "The Server U finished sending the response and the updated room status to the main server." << endl;
                }
                else
                {
                    cout << "Cannot make a reservation. Room " << roomCode << " is not available." << endl;
                    sendUDP("No reserve", "44489");
                    cout << "The Server U finished sending the response to the main server." << endl;
                }
            }
        }
    } 
    return 0;
}
