#include <iostream>
#include <thread>
#include <netinet/in.h>
#include <cstring>
#include <libnet.h>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "utils.h"

using namespace std;

string myIp, myPort;

// <hash, list-of-seeders-and-leechers>
unordered_map<string, unordered_set<string>> seedersAndLeechers;

void fillFieldsFromMsg(const string &msgBuffer, string &commandType, string &shaOfSha, string &clientIP,
                       string &clientPort) {
    vector<string> tokens = getTokens(msgBuffer, FIELD_SEPARATOR);

    if (tokens.empty()) {
        clog << "Tokens empty" << endl;
        return;
    }
    commandType = !tokens.empty() ? tokens[0] : "";
    shaOfSha = tokens.size() > 1 ? tokens[1] : "";
    clientIP = tokens.size() > 2 ? tokens[2] : "";
    clientPort = tokens.size() > 3 ? tokens[3] : "";
}

void notifyOtherTracker(const string &syncMsg, const string &otherTrackerSocket) {
    auto tokens = getTokens(otherTrackerSocket, ':');
    int sockfd;
    struct sockaddr_in servaddr{};

    // Creating socket file descriptor
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        printf("socket creation failed");
        exit(0);
    }

    memset(&servaddr, 0, sizeof(servaddr));

    // Filling server information
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr(tokens[0].c_str());
    servaddr.sin_port = htons(stoi(tokens[1]));
    // send hello message to server
    sendto(sockfd, (const char *) syncMsg.c_str(), strlen(syncMsg.c_str()),
           0, (const struct sockaddr *) &servaddr,
           sizeof(servaddr));
}


void serveTracker(int udpfd, struct sockaddr_in cliaddr) {
    char buffer[MAX_DATAGRAM_SIZE];
    socklen_t addr_size = sizeof(cliaddr);
    bzero(buffer, sizeof(buffer));

    recvfrom(udpfd, buffer, sizeof(buffer), 0,
             (struct sockaddr *) &cliaddr, &addr_size);

    cout << "Incoming request over UDP from tracker: "
         << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port) << endl
         << "\t\t msg: " << buffer << endl;

    string msgType, shaOfSha, clientIP, clientPort;
    fillFieldsFromMsg(buffer, msgType, shaOfSha, clientIP, clientPort);

    if (msgType == MSG_ADD) {
        unordered_set<string> uset = seedersAndLeechers[shaOfSha];
        uset.insert(clientIP + ":" + clientPort);
        seedersAndLeechers[shaOfSha] = uset;

    } else if (msgType == MSG_REMOVE) {
        unordered_set<string> uset = seedersAndLeechers[shaOfSha];
        uset.erase(clientIP + ":" + clientPort);
        seedersAndLeechers[shaOfSha] = uset;
    }
}

void serveClient(int newsocketFD, struct sockaddr_in newAddr, const string &otherTrackerSocket) {
    cout << "Inside serveClient" << endl;
    cout << "Received request from: "
         << inet_ntoa(newAddr.sin_addr) << ":" << ntohs(newAddr.sin_port) << endl;

    int msgLength;
    recv(newsocketFD, &msgLength, sizeof(msgLength), 0);
    cout << "msgLength: " << msgLength << endl;

    char msgBuffer[msgLength + 1];
    recv(newsocketFD, msgBuffer, msgLength, 0);

    msgBuffer[msgLength] = '\0';

    cout << "msgBuffer: " << msgBuffer << endl;


    string msgType, shaOfSha, clientIP, clientPort;

    fillFieldsFromMsg(msgBuffer, msgType, shaOfSha, clientIP, clientPort);

    if (msgType == MSG_ADD) {
        unordered_set<string> uset = seedersAndLeechers[shaOfSha];
        uset.insert(clientIP + ":" + clientPort);
        seedersAndLeechers[shaOfSha] = uset;

    } else if (msgType == MSG_REMOVE) {
        unordered_set<string> uset = seedersAndLeechers[shaOfSha];
        uset.erase(clientIP + ":" + clientPort);
        seedersAndLeechers[shaOfSha] = uset;

    } else if (msgType == MSG_SEEDERLIST) {
        // Returns list of seedersAndLeechers for given file
        unordered_set<string> uset = seedersAndLeechers[shaOfSha];
        // TODO: construct a string to be returned
        string response;
        for (const auto &e: uset) {
            response.append(e);
            response += FIELD_SEPARATOR;
        }
        cout << "Response to be sent to client: " << response << endl;
        int responseLength = response.length();
        send(newsocketFD, &responseLength, sizeof(responseLength), 0);
        send(newsocketFD, response.c_str(), responseLength, 0);
    }

    cout << "Updated seedersAndLeechers list" << endl;
    for (const auto& e: seedersAndLeechers) {
        cout << "\t" << e.first << endl;
        for (const auto& p:e.second) {
            cout << "\t\t" << p << endl;
        }
        cout << endl;
    }

    cout << "Ending connection with: "
         << inet_ntoa(newAddr.sin_addr) << ":" << ntohs(newAddr.sin_port) << endl;

    close(newsocketFD);

    cout << "Sending sync msg to other tracker";
    if (msgType == MSG_ADD or msgType == MSG_REMOVE) {
        // TODO: Notify other tracker
        notifyOtherTracker(msgBuffer, otherTrackerSocket);
    }
}

void tcpUdp(const string &otherTrackerSocket) {
    int listenFD;
    int udpFD;
    int connFD;

    socklen_t addr_size;
    int maxfdp1;
    fd_set rset;

    struct sockaddr_in serveraddr{}, cliaddr{};

    // Create TCP socket for listening
    listenFD = socket(AF_INET, SOCK_STREAM, 0);

    if (listenFD < 0) {
        cerr << "Failed to create socket" << endl;
        exit(1);
    }

    memset(&serveraddr, 0, sizeof(serveraddr));// bzero(&serveraddr, sizeof(&serveraddr))

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(stoi(myPort));
    serveraddr.sin_addr.s_addr = inet_addr(myIp.c_str());

    // binding server addr structure to listenFD
    if (bind(listenFD, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        cerr << "Error in binding to " << endl;
        exit(1);
    }

    if (listen(listenFD, MAX_PENDING_REQUESTS) < 0) {
        cerr << "Error in listen errno: " << errno << endl;
        exit(1);
    }


    /* create UDP socket */
    udpFD = socket(AF_INET, SOCK_DGRAM, 0);
    // binding server addr structure to udp sockfd
    bind(udpFD, (struct sockaddr *) &serveraddr, sizeof(serveraddr));

    clog << "Listening on port for TCP and UDP: " << myPort << endl;

    // clear the descriptor set
    FD_ZERO(&rset);

    // get maxfd
    maxfdp1 = max(listenFD, udpFD) + 1;


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (true) {
        // Infinite loop Accepts incoming requests from clients

        // add listenfd and udpfd to readset
        FD_SET(listenFD, &rset);
        FD_SET(udpFD, &rset);

        // select the ready descriptor
        // select() and pselect() allow a program to  monitor  multiple  file  de‐
        //       scriptors,  waiting  until  one  or more of the file descriptors become
        //       "ready" for some class of I/O operation (e.g., input possible).  A file
        //       descriptor  is  considered  ready if it is possible to perform a corre‐
        //       sponding  I/O  operation
        //       Returns number of ready descriptors
        select(maxfdp1, &rset, nullptr, nullptr, nullptr);

        // if tcp socket is readable then handle
        // it by accepting the connection
        if (FD_ISSET(listenFD, &rset)) {
            addr_size = sizeof(cliaddr);

            connFD = accept(listenFD, (struct sockaddr *) &cliaddr, &addr_size);
            clog << "Accepted new request " << endl;
            if (connFD >= 0) {
                // Process the request on separate thread
                thread serveClientThread(serveClient, connFD, cliaddr, otherTrackerSocket);
                serveClientThread.detach();
            }
        }

        // if udp socket is readable receive the message.
        if (FD_ISSET(udpFD, &rset)) {
            addr_size = sizeof(cliaddr);
            thread serverTrackersThread(serveTracker, udpFD, cliaddr);
            serverTrackersThread.detach();
        }
    }
#pragma clang diagnostic pop
}

int main(int argc, char **argv) {
    auto tokens = getTokens(argv[1], ':');
    myIp = tokens[0];
    myPort = tokens[1];
    string otherTrackerSocket = argv[2];

    thread tcpUdpThread(tcpUdp, otherTrackerSocket);

    tcpUdpThread.join();
}