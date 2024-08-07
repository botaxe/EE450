#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <csignal>
#include <map>
#include <sstream>
#include <fstream>
#include <string>
#include <ctype.h>

using namespace std;

#define MAXDATASIZE 100
map<string, int> allRoomInfo;       // Map to store all room info from all backend servers
map<string, string> encryptedUsers; // Map to store encrypted usernames and passwords
string username; 

// called with member.txt to store it in encryptedUsers
map<string, string> readEncryptedUsername(const char *fileName) {
    ifstream file(fileName);
    string data;
    map<string, string> allUsers;
    if (!file.is_open()) {
        cout << "can't open file." << endl;
        return allUsers;
    }
    while (getline(file, data)) {
        stringstream ss(data);
        string username;
        string password;
        if (!(getline(ss, username, ',') && ss >> password)) {
            cout << "Failed to parse line: " << data << endl;
            continue;
        }
        // Convert username to lowercase
        for (size_t i = 0; i < username.length(); ++i) {
            username[i] = tolower(username[i]);
        }
        allUsers[username] = password;
    }
    file.close();
    return allUsers;
}
// method to check login combination and return message delivered back to client
string checkLogin(string username, string password) {
    map<string, string>::iterator it = encryptedUsers.find(username);
    if (it == encryptedUsers.end()) {
        return "noUsername";
    }
    if (it->second == password) {
        return "valid";
    }
    return "passwordMismatch";
}

// Function to send UDP message from beej broadcaster.c
void sendUDP(const string &data, const string &port) {
    int udpSock;
    struct sockaddr_in broadcast_addr;
    // UDP socket setup
    if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        // perror("Error: socket");
        return;
    }
    int broadcast = 1;
    if (setsockopt(udpSock, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        // perror("Error: setsockopt (broadcast)");
        close(udpSock);
        return;
    }
    memset(&broadcast_addr, 0, sizeof(broadcast_addr));
    broadcast_addr.sin_family = AF_INET;
    broadcast_addr.sin_port = htons(atoi(port.c_str()));     // Convert port string to integer
    broadcast_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    if (sendto(udpSock, data.c_str(), data.length(), 0, (struct sockaddr *)&broadcast_addr, sizeof(broadcast_addr)) == -1) {
        // perror("sendto");
    }
    close(udpSock);
}

// Function to receive UDP messages and return the received message from beej listener.c
string receiveUDP(const string &port) {
    int udpSock;
    struct sockaddr_in my_addr, their_addr;
    socklen_t addr_len;
    char buf[MAXDATASIZE];
    // UDP socket setup
    if ((udpSock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
        // perror("socket");
        return "";
    }
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(atoi(port.c_str())); // Convert port string to integer
    my_addr.sin_addr.s_addr = INADDR_ANY;
    memset(my_addr.sin_zero, '\0', sizeof(my_addr.sin_zero));
    // Bind the socket only if port is not 0
    if (atoi(port.c_str()) != 0) {
        if (bind(udpSock, (struct sockaddr *)&my_addr, sizeof(my_addr)) == -1) {
            // perror("bind");
            close(udpSock);
            return "";
        }
    }
    string receivedMessage;
    addr_len = sizeof(their_addr);
    memset(buf, 0, sizeof(buf)); // Clear the buffer before receiving a message
    if (recvfrom(udpSock, buf, MAXDATASIZE - 1, 0, (struct sockaddr *)&their_addr, &addr_len) == -1) {
        // perror("recvfrom");
        close(udpSock);
        return "";
    }
    receivedMessage = buf;
    close(udpSock);
    return receivedMessage;
}

// Function to setup TCP server socket Beej all of chapter 6
int setup_tcp_server_socket(int port) {
    int server_sock;
    // Create TCP socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        // perror("TCP socket creation failed");
        exit(1);
    }
    // Allow reuse of the port
    int opt = 1;
    if (setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        // perror("setsockopt");
        exit(1);
    }
    // Set up server address
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    // Bind socket to port
    if (bind(server_sock, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        // perror("bind");
        exit(1);
    }
    // Listen for incoming connections
    if (listen(server_sock, 10) < 0) {
        // perror("listen");
        exit(1);
    }
    return server_sock;
}

// Function to handle TCP client connection - how to use fork from https://www.geeksforgeeks.org/create-processes-with-fork-in-cpp/
void handle_tcp_client_connection(int client_sock, int serverM_tcp) {
    int pid = fork();
    if (pid == -1) { // no child process created
        // perror("Fork error");
        exit(1);
    } else if (pid == 0) { // Child process
        // Close the listening socket in the child process
        close(serverM_tcp);
        char buffer[MAXDATASIZE];
        ssize_t numbytes;
        while (true) {
            // Receive message from the client
            if ((numbytes = recv(client_sock, buffer, MAXDATASIZE - 1, 0)) == -1) {
                // perror("recv");
                exit(1);
            }
            buffer[numbytes] = '\0'; // Null-terminate the received data
            
            string roomCode(buffer);
            // Check if room code exists in allRoomInfo
            if (roomCode.find("A::") != string::npos || roomCode.find("R::") != string::npos) {
                string code = roomCode.substr(6);
                size_t roomCodeIndex = roomCode.find("::") + 2;
                char server_identifier = code[0]; // Get the first character of the room code as the server identifier
                string destinationPort;
                if (server_identifier == 'S') {
                    destinationPort = "41489";
                } else if (server_identifier == 'D') {
                    destinationPort = "42489";
                } else if (server_identifier == 'U') {
                    destinationPort = "43489";
                }
                if (roomCode.find("A::") != string::npos) { // If it's an availability request
                    cout << "The main server has received the availability request on Room " << code << " from " << username << " using TCP over port 45489." << endl;
                    sendUDP(roomCode.substr(roomCodeIndex), destinationPort);
                    cout << "The main server sent a request to Server " << server_identifier << "." << endl;
                    string udpMessage = receiveUDP("44489");
                    const char *message = udpMessage.c_str();
                    cout << "The main server received the response from Server " << server_identifier << " using UDP over port 44489." << endl;
                    send(client_sock, message, strlen(message), 0);
                    cout << "The main server sent the availability information to the client." << endl;
                } else { // Reservation request
                    if (roomCode.find("N::") != string::npos) {// If its a guest asking for reservation
                        cout << username << " cannot make a reservation." << endl;
                        cout << "The main server sent the error message to the client." << endl;
                        string errorMessage = "No permission";
                        const char *eMessage = errorMessage.c_str();
                        send(client_sock, eMessage, strlen(eMessage), 0);
                    } else { 
                        cout << "The main server has received the reservation request on Room " << code << " from " << username << " using TCP over port 44489." << endl;
                        sendUDP(roomCode.substr(roomCodeIndex), destinationPort);
                        cout << "The main server sent a request to Server " << server_identifier << "." << endl;
                        string udpMessage = receiveUDP("44489");
                        if (udpMessage == "Reserve") {
                            cout << "The main server received the response and the updated room status from Server " << server_identifier << " using UDP over port 44489." << endl;
                            cout << "The room status of Room " << code << " has been updated." << endl;
                        } else {
                            cout << "The main server received the response from Server " << server_identifier << " using UDP over port 44489." << endl;
                        }
                        const char *message = udpMessage.c_str();
                        send(client_sock, message, strlen(message), 0);
                        cout << "The main server sent the reservation result to the client." << endl;
                    }
                }
            }  else { // if its not a query in the format role::action::roomcode we know its username and password so we can parse
                // Parsing username and password from the received message
                size_t commaPosition1 = roomCode.find(',');
                size_t commaPosition2 = roomCode.find(',', commaPosition1 + 1);
                string enc_username, enc_password;

                if (commaPosition1 != string::npos) {
                    enc_username = roomCode.substr(0, commaPosition1);
                }
                if (commaPosition2 != string::npos) {
                    username = roomCode.substr(commaPosition1 + 1, commaPosition2 - commaPosition1 - 1);
                    enc_password = roomCode.substr(commaPosition2 + 1);
                } else {
                    username = roomCode.substr(commaPosition1 + 1);
                    enc_password = "";
                }
                if (enc_password.empty()) {
                    cout << "The main server received the guest request for " << username << " using TCP over port 45489. The main server accepts " << username << " as a guest." << endl;
                    cout << "The main server sent the guest response to the client." << endl;
                    const char *response = "success";
                    send(client_sock, response, strlen(response), 0);
                } else  {
                    string status = checkLogin(enc_username, enc_password);
                    if (status == "valid") {
                        cout << "The main server received the authentication for " << username << " using TCP over port 45489." << endl;
                        cout << "The main server sent the authentication result to the client." << endl;
                        const char *response = "success";
                        send(client_sock, response, strlen(response), 0);
                    } else if (status == "noUsername") {
                        const char *response = "noUsername"; // Login failed
                        send(client_sock, response, strlen(response), 0);
                    } else {
                        const char *response = "passwordMismatch"; // Login failed
                        send(client_sock, response, strlen(response), 0);
                    }
                }
            }
        }
        // Close the client socket in the child process
        close(client_sock);
        // Terminate the child process
        exit(0);
    } else { // Parent process
        // Close the client socket in the parent process
        close(client_sock);
    }
}

int main(void) {
    // Setup TCP server
    int serverM_tcp = setup_tcp_server_socket(45489);
    cout << "The main server is up and running." << endl;
    bool received_S = false, received_D = false, received_U = false;
    while (!(received_S && received_D && received_U)) {
        // Check for UDP messages
        string udpMessage = receiveUDP("44489");
        if (!udpMessage.empty()) {
            // Process the received UDP message
            string server_source;
            if (udpMessage[0] == 'S') {
                server_source = "S";
                received_S = true;
            } else if (udpMessage[0] == 'D') {
                server_source = "D";
                received_D = true;
            }  else if (udpMessage[0] == 'U') {
                server_source = "U";
                received_U = true;
            } else {
                continue;
            }
            stringstream ss(udpMessage);
            string roomCode;
            int numRoom;
            while (getline(ss, roomCode, ',') && ss >> numRoom) {
                allRoomInfo[roomCode] = numRoom;
            }
            cout << "The main server has received the room status from Server " << server_source << " using UDP over port 44489." << endl;
        }
    }

    // Read encrypted usernames and passwords from file
    encryptedUsers = readEncryptedUsername("./member.txt");
    // From select.c in Beej
    while (true) {
        fd_set readfds;
        FD_ZERO(&readfds);

        // Add TCP server socket
        FD_SET(serverM_tcp, &readfds);
        int max_sd = serverM_tcp;

        select(max_sd + 1, &readfds, NULL, NULL, NULL);
        // Handle TCP connections
        if (FD_ISSET(serverM_tcp, &readfds)) {
            struct sockaddr_in client_addr;
            socklen_t sin_size = sizeof(client_addr);
            int new_fd = accept(serverM_tcp, (struct sockaddr *)&client_addr, &sin_size);
            handle_tcp_client_connection(new_fd, serverM_tcp);
        }
    }
    // Close TCP socket
    close(serverM_tcp);
    return 0;
}
