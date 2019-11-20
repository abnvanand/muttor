#include <iostream>
#include <thread>
#include <netinet/in.h>
#include <cstring>
#include <libnet.h>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include "utils.h"


using namespace std;

// <hash, list-of-seeders-and-leechers>
unordered_map<string, unordered_set<string>> seedersAndLeechers;
// <hash, list-of-leechers>
//unordered_map<string, unordered_set<string>> leechers;


void synchronizeTrackers() {

}

void fillFieldsFromMsg(string msgBuffer, string &commandType, string &shaOfSha, string &clientIP, string &clientPort) {
    vector<string> tokens = getTokens(msgBuffer, FIELD_SEPARATOR);

    if (tokens.empty()) {
        clog << "Tokens empty" << endl;
        return;
    }
    commandType = tokens.size() > 0 ? tokens[0] : "";
    shaOfSha = tokens.size() > 1 ? tokens[1] : "";
    clientIP = tokens.size() > 2 ? tokens[2] : "";
    clientPort = tokens.size() > 3 ? tokens[3] : "";
}

void serveClient(int newsocketFD, struct sockaddr_in newAddr) {
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

    if (msgType == MSG_ADD or msgType == MSG_REMOVE) {
        // TODO: Notify other tracker
//        updateSeederLeecherStatusOnTracker(msgType, shaOfSha);
    }

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
        for (auto e: uset) {
            response.append(e);
            response += FIELD_SEPARATOR;
        }
        cout << "Response to be sent to client: " << response << endl;
        int responseLength = response.length();
        send(newsocketFD, &responseLength, sizeof(responseLength), 0);
        send(newsocketFD, response.c_str(), responseLength, 0);
    }

    cout << "Updated seedersAndLeechers list" << endl;
    for (auto e: seedersAndLeechers) {
        cout << "\t" << e.first << endl;
        for (auto p:e.second) {
            cout << "\t\t" << p << endl;
        }
        cout << endl;
    }

    cout << "Ending connection with: "
         << inet_ntoa(newAddr.sin_addr) << ":" << ntohs(newAddr.sin_port) << endl;

    close(newsocketFD);
}

void clientComm(int myPort) {
    // TODO: switch to UDP "SOCK_DGRAM"
    clog << "Here port" << myPort << endl;

    int socketFD;
    struct sockaddr_in serveraddr;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);

    if (socketFD < 0) {
        cerr << "Failed to create socket" << endl;
        exit(1);
    }

    memset(&serveraddr, 0, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(myPort);
    serveraddr.sin_addr.s_addr = inet_addr(LOCALHOST);

    if (bind(socketFD, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        cerr << "Error in binding to " << endl;
        exit(1);
    }

    if (listen(socketFD, MAX_PENDING_REQUESTS) < 0) {
        cerr << "Error in listen errno: " << errno << endl;
        exit(1);
    }

    clog << "Listening on port: " << myPort << endl;

    struct sockaddr_in newAddr;
    int newsocketFD;
    socklen_t addr_size;
    addr_size = sizeof(newAddr);


#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
    while (true) {
        // Infinite loop Accepts incoming requests from clients

        newsocketFD = accept(socketFD, (struct sockaddr *) &newAddr, &addr_size);
        clog << "Accepted new request " << endl;

        if (newsocketFD >= 0) {
            // Process the request on separate thread
            thread serveClientThread(serveClient, newsocketFD, newAddr);
            serveClientThread.detach();
        }
    }
#pragma clang diagnostic pop
}

int main(int argc, char **argv) {
    string myPort = argv[1];

    // TODO: Implement Multitracker system
    thread syncThread(synchronizeTrackers);

    thread clientCommThread(clientComm, stoi(myPort));

    syncThread.join();
    clientCommThread.join();
}