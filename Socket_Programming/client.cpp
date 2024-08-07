#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <map>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <ctype.h>

#define MAXDATASIZE 100 
using namespace std;
string role = ""; // N for guest and M for member

// Login prompt user
void login(char *username, char *password)
{
    cout << "Please enter the username: ";
    cin.getline(username, MAXDATASIZE);

    cout << "Please enter the password: ";
    cin.getline(password, MAXDATASIZE);
}
// encryption algorithm 
string encrypt(string password)
{
    string encrypted = password; 

    for (size_t i = 0; i < password.length(); ++i)
    {
        if (isalpha(password[i]))
        {
            char base = isupper(password[i]) ? 'A' : 'a';
            encrypted[i] = ((password[i] - base + 3) % 26) + base;
        }
        else if (isdigit(password[i]))
        {
            encrypted[i] = ((password[i] - '0' + 3) % 10) + '0';
        }
    }
    return encrypted;
}

string encryptUser(string username)
{
    string encrypted = username;

    for (size_t i = 0; i < username.length(); ++i)
    {
        encrypted[i] = tolower(username[i]);
    }
    for (size_t i = 0; i < encrypted.length(); ++i)
    {
        if (isalpha(encrypted[i]))
        {
            char base = 'a';
            encrypted[i] = ((encrypted[i] - base + 3) % 26) + base;
        }
        else if (isdigit(encrypted[i]))
        {
            encrypted[i] = ((encrypted[i] - '0' + 3) % 10) + '0';
        }
    }
    return encrypted;
}
// First half setting up TCP from server.c in Beej
int main()
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    char username[50], password[50]; // max length of username and password is ~50 as part of constraints

    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage my_addr;
    socklen_t addrlen = sizeof(my_addr);

    cout << "Client is up and running." << endl;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo("127.0.0.1", "45489", &hints, &servinfo);

    // loop through all the results and connect to the first we can
    for (p = servinfo; p != NULL; p = p->ai_next)
    {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            // perror("socket");
            continue;
        }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            // perror("connect");
            continue;
        }

        break;
    }

    if (p == NULL)
    {
        // fprintf(stderr, "client: failed to connect\n");
        return 0;
    }
    freeaddrinfo(servinfo); // all done with this structure

    // Get port number dynamically...
    int getsock_check = getsockname(sockfd, (struct sockaddr *)&my_addr, &addrlen);
    if (getsock_check == -1)
    {
        // perror("getsockname");
        exit(1);
    }
    struct sockaddr_in *my_addr_ipv4 = (struct sockaddr_in *)&my_addr;
    int port_number = ntohs(my_addr_ipv4->sin_port);

    // Testing Login information from console
    bool loggedIn = false;
    while (!loggedIn)
    {
        login(username, password);
        string encryptedUsername = encryptUser(username);
        string encryptedPassword = encrypt(password);
        if (encryptedPassword.length() == 0)
        {
            cout << username << " sent a guest request to the main server using TCP over port " << port_number << "." << endl;
        }
        else
        {
            cout << username << " sent an authentication request to the main server." << endl;
        }
        // Concatenate the encrypted username and password with the original username
        string userpass = encryptedUsername + "," + string(username) + "," + encryptedPassword;

        send(sockfd, userpass.c_str(), userpass.length(), 0);
        numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0);
        buf[numbytes] = '\0';
        if (strcmp(buf, "success") == 0)
        {
            if (strlen(password) == 0)
            {
                cout << "Welcome guest " << username << "!" << endl;
                role = "N";
            }
            else
            {
                cout << "Welcome member " << username << "!" << endl;
                role = "M";
            }
            loggedIn = true; 
        }
        else if (strcmp(buf, "passwordMismatch") == 0)
        {
            cout << "Failed login: Password does not match." << endl;
        }
        else if (strcmp(buf, "noUsername") == 0)
        {
            cout << "Failed login: Username does not exist." << endl;
        }
    }

    // Prompt for room code and send query to main server
    while (true)
    {
        string currentQuery;
        string action;
        // Prompt for room code
        cout << "Please enter the room code: ";
        cin >> currentQuery;
        while (!(currentQuery[0] == 'S' || currentQuery[0] == 'D' || currentQuery[0] == 'U')) {
            cout << "Invalid room code. Room code must start with S, D, or U. Please try again: ";
            cin >> currentQuery;
        }

    // Prompt for action
        cout << "Would you like to search for the availability or make a reservation? (Enter 'Availability' or 'Reservation'): ";
        cin >> action;
        while (!(action == "Availability" || action == "Reservation")) {
            cout << "Invalid action. Please enter 'Availability' or 'Reservation': ";
            cin >> action;
        }

        string message = role + "::" + action.substr(0, 1) + "::" + currentQuery;

        send(sockfd, message.c_str(), message.length(), 0);

        numbytes = recv(sockfd, buf, MAXDATASIZE - 1, 0);
        buf[numbytes] = '\0';

        if (numbytes > 0)
        {
            if (action.substr(0, 1) == "A") {
                cout << username << " sent an availability request to the main server." << endl;
            } else {
                cout << username << " sent a reservation request to the main server." << endl;
            }
            cout << "The client received the response from the main server using TCP over port " << port_number << "." << endl;

            if (role == "M" && action.substr(0, 1) == "A")
            {
                if (strcmp(buf, "Avail") == 0)
                {
                    cout << "The requested room is available." << endl << endl;
                }
                else if (strcmp(buf, "Not avail") == 0)
                {
                    cout << "The requested room is not available." << endl << endl;
                }
                else if (strcmp(buf, "No room") == 0)
                {
                    cout << "Not able to find the room layout." << endl << endl;
                }
            }
            else if (role == "M" && action.substr(0, 1) == "R")
            {
                if (strcmp(buf, "Reserve") == 0)
                {
                    cout << "Congratulation! The reservation for Room " << currentQuery << " has been made." << endl << endl;
                }
                else if (strcmp(buf, "No reserve") == 0)
                {
                    cout << "Sorry! The requested room is not available." << endl << endl;
                }
                else if (strcmp(buf, "No room") == 0)
                {
                    cout << "Oops! Not able to find the room." << endl << endl;
                }
            }
            else if (role == "N" && action.substr(0, 1) == "A")
            {
                if (strcmp(buf, "Avail") == 0)
                {
                    cout << "The requested room is available." << endl << endl;
                }
                else if (strcmp(buf, "Not avail") == 0)
                {
                    cout << "The requested room is not available." << endl << endl;
                }
                else if (strcmp(buf, "No room") == 0)
                {
                    cout << "Not able to find the room layout." << endl << endl;
                }
            }
            else
            {
                cout << "Permission denied: Guest cannot make a reservation." << endl << endl;
            }
            cout << "-----Start a new request-----" << endl;
        }
    }
    close(sockfd);
    return 0;
}
