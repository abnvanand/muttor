#include <iostream>
#include <thread>
#include <vector>
#include <cassert>
#include <cmath>
#include <cstring>
#include <unordered_map>
#include <netinet/in.h>
#include <libnet.h>
#include <algorithm>

#include "utils.h"

#define CMD_SHARE "share"
#define CMD_GET "get"

#define REQUEST_BITVECTOR 100
#define REQUEST_DATA 101
#define QUIT "q"
using namespace std;

string myIp, myPort, defaultTracker;
// <hash, bitvector>
unordered_map<string, string> hashPieces;
// <hash, localFilePath>
unordered_map<string, string> hashPath;

void updateSeederLeecherStatusOnTracker(string msgType, string shaOfSha, vector<string> &trackers);

void servePeerRequest(int newsocket, struct sockaddr_in newAddr) {
    cout << "Received request from PEER: "
         << inet_ntoa(newAddr.sin_addr) << ":" << ntohs(newAddr.sin_port) << endl;

    int requestType;
    recv(newsocket, &requestType, sizeof(requestType), 0);
    if (requestType == REQUEST_BITVECTOR) {
        char shaOfShaBuffer[SHA_DIGEST_LENGTH + 1];

        recv(newsocket, &shaOfShaBuffer, SHA_DIGEST_LENGTH, 0);
        shaOfShaBuffer[SHA_DIGEST_LENGTH] = '\0';

        string shaOfSha = (string) shaOfShaBuffer;

        cout << "Peer requesting for bitvector of " << shaOfSha << endl;

        string bitVector = hashPieces[shaOfSha];

        send(newsocket, bitVector.c_str(), bitVector.size(), 0);

    } else if (requestType == REQUEST_DATA) {
        char shaOfShaBuffer[SHA_DIGEST_LENGTH + 1];

        recv(newsocket, &shaOfShaBuffer, SHA_DIGEST_LENGTH, 0);
        shaOfShaBuffer[SHA_DIGEST_LENGTH] = '\0';
        string shaOfSha = (string) shaOfShaBuffer;

        int pieceId = -1;
        recv(newsocket, &pieceId, sizeof(pieceId), 0);

        cout << "Peer requesting for piece: " << pieceId
             << "  of file: " << hashPath[shaOfSha]
             << " with hash: " << shaOfSha << endl;


        int fileSize = getFileSize(hashPath[shaOfSha]);
        int numPieces = ceil((float) fileSize / (float) PIECE_SIZE);
        bool isLastPiece = pieceId == numPieces - 1;
        int currentPieceSize = isLastPiece ?
                               fileSize - ((numPieces - 1) * PIECE_SIZE) :
                               PIECE_SIZE;

        // Each individual piece is sent in small chunks
        int numChunks = ceil((float) currentPieceSize / (float) CHUNK_SIZE);
        send(newsocket, &numChunks, sizeof(numChunks), 0);

        int pieceOffset = pieceId * PIECE_SIZE;

        ifstream infile(hashPath[shaOfSha], ios_base::binary);
        // Seek to beginning of requested piece
        infile.seekg(pieceOffset);

        for (int chunkid = 0; chunkid < numChunks; chunkid++) {
            cout << "\t Sending chunk: " << chunkid << " of piece: " << pieceId << endl;

            bool isLastChunk = chunkid == numChunks - 1;
            int currentChunkSize = isLastChunk ?
                                   currentPieceSize - ((numChunks - 1) * CHUNK_SIZE) :
                                   CHUNK_SIZE;
            char chunkBuffer[currentChunkSize + 1];
            memset(chunkBuffer, '\0', currentChunkSize + 1);

            infile.read(chunkBuffer, currentChunkSize);

            cout << "\t\t  chunk: " << chunkid << " size: " << currentChunkSize << endl;
//            cout << "\t\t  chunk: " << chunkid << " data: " << chunkBuffer << endl;

            send(newsocket, &currentChunkSize, sizeof(currentChunkSize), 0);
            send(newsocket, &chunkBuffer, currentChunkSize, 0);

            cout << "\t Sent chunk: " << chunkid << " of piece: " << pieceId << endl;

            //  Implement ack based handing
            //  Since send() is non blocking the peer sends too many buffers to
            //  requester whereas receive is blocking and the pending request queue at receiver overflows
            //  leading to segfault.
            //  sleeping is a bad way of allowing receiver enough time to process the packet
            //  a better way would be to implement ack based communication flow
            //  where sender waits for the ack of previously sent packet before sending more packets
//            sleep(1);

            // Wait for acknowledgement of this chunk
            int ack = 0;
            recv(newsocket, &ack, sizeof(ack), 0);
            cout << "\t Received ack for chunk: " << chunkid << " of piece: " << pieceId << endl;
            cout << "Total Pieces: " << numPieces << endl;
        }

        infile.close();
    }
    close(newsocket);
}

void peerServer() {
    int socketFD;
    struct sockaddr_in serveraddr;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0) {
        cout << "Error in connect" << endl;
        exit(1);
    }

    memset(&serveraddr, '\0', sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_port = htons(stoi(myPort));
    // Can't use INADDR_ANY bcoz have to tell my ip to trackers so that other peers could find me
    serveraddr.sin_addr.s_addr = inet_addr(myIp.c_str());

    if (bind(socketFD, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) {
        cout << "Error in bind" << endl;
        exit(1);
    }

    if (listen(socketFD, MAX_PENDING_REQUESTS) < 0) {
        cout << "Error in listen" << endl;
        exit(1);
    }

    struct sockaddr_in newAddr;
    int newsocket;
    socklen_t addr_size;
    addr_size = sizeof(newAddr);

    while (true) {
        newsocket = accept(socketFD, (struct sockaddr *) &newAddr, &addr_size);

        if (newsocket >= 0) {
            thread peerThread(servePeerRequest,
                              newsocket,
                              newAddr);
            peerThread.detach();
        }
    }

}

void fillFieldsFromMtorrentFile(string mTorrentFilePath,
                                vector<string> &trackers,
                                string &fileSize,
                                string &hash) {
    // Hash = 20 char sha1 of each piece
    ifstream infile(mTorrentFilePath);
    getline(infile, fileSize); // skips filename
    getline(infile, fileSize);
    getline(infile, hash);

    string tracker;
    while (getline(infile, tracker)) {
        if (tracker.empty())
            continue;
        trackers.push_back(tracker);
    }

    cout << "Read from " << mTorrentFilePath << endl;
    cout << " fileSize " << fileSize << endl;
    cout << " hash " << hash << endl;
    for (int i = 0; i < (int) trackers.size(); i++)
        cout << " tracker" << i + 1 << " " << trackers[i] << endl;

    infile.close();
}

string getSeedersFromTracker(string shaOfSha, vector<string> &trackers) {
    cout << "Inside getSeedersFromTracker" << endl;
    int socketFD;
    struct sockaddr_in serverAddress;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);// TODO: switch to UDP
    if (socketFD < 0) {
        cerr << "\tError in creating Socket" << endl;
        exit(1);
    }

    bool connectedToTracker = false;

    for (auto trackerSocketAddr: trackers) {
        vector<string> tokens = getTokens(trackerSocketAddr, ':');
        assert(!tokens.empty());

        memset(&serverAddress, '\0', sizeof(serverAddress));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(stoi(tokens[1]));
        serverAddress.sin_addr.s_addr = inet_addr(tokens[0].c_str());

        cout << "\tConnecting to  tracker: " << trackerSocketAddr << endl;
        if (connect(socketFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
            cout << "\tTracker: " << trackerSocketAddr << " is down." << endl;
        } else {
            connectedToTracker = true;
            break;
        }
    }
    if (!connectedToTracker) {
        cout << "\tAll trackers are down" << endl;
        exit(1);
    }

    stringstream ss;
    ss << MSG_SEEDERLIST << FIELD_SEPARATOR;
    ss << shaOfSha << FIELD_SEPARATOR;
    ss << myIp << FIELD_SEPARATOR;
    ss << myPort << FIELD_SEPARATOR;

    string msg = ss.str();

    int msgLength = msg.length();

    // First send size of the msg to be received by tracker
    send(socketFD, &msgLength, sizeof(msgLength), 0);
    // Send the actual msg
    send(socketFD, msg.c_str(), msgLength, 0);

    int responseLength;
    recv(socketFD, &responseLength, sizeof(responseLength), 0);

    char responseBuffer[responseLength + 1];
    recv(socketFD, responseBuffer, msgLength, 0);

    responseBuffer[responseLength] = '\0';

    cout << "\tReceived response: " << responseBuffer << endl;

    close(socketFD);

    return responseBuffer;
}

void downloadPeerPieceInfo(string peerSocketAddr, string shaOfSha, string &peerBitVector) {
    cout << "Connecting to peer " << peerSocketAddr << endl;

    // tokens[0] = IP Address, tokens[1] = Port
    vector<string> tokens = getTokens(peerSocketAddr, ':');
    int socketFD;
    struct sockaddr_in serverAddr;

    // Connect to peer over TCP
    socketFD = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFD < 0) {
        cerr << "Socket error: " << errno << endl;
        exit(1);
    }
    memset(&serverAddr, '\0', sizeof(serverAddr));

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(stoi(tokens[1]));
    serverAddr.sin_addr.s_addr = inet_addr(tokens[0].c_str());

    if (connect(socketFD, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
        cout << "Connect error: " << errno << endl;
        exit(1);
    }

    int requestType = REQUEST_BITVECTOR;
    send(socketFD, &requestType, sizeof(int), 0);
    send(socketFD, shaOfSha.c_str(), shaOfSha.length(), 0);

    char bufferVector[peerBitVector.length() + 1];

    recv(socketFD, bufferVector, peerBitVector.length(), 0);
    bufferVector[peerBitVector.length()] = '\0';

    peerBitVector = bufferVector;

    close(socketFD);
}

bool rarestFirstCmp(pair<int, vector<int>> &a, pair<int, vector<int>> &b) {
    return a.second.size() < b.second.size();
}

void pieceSelectionAlgo(vector<string> &peerList,
                        vector<string> &peerBitVectors,
                        vector<vector<int>> &peerToPiecesMap) {
    // TODO: Implement rarest first algo

    // Peers are 0 based indexed
    // Pieces are 0 based indexed

    // Currently first peer found with the piece is set as piece provider
    int numPeers = peerBitVectors.size();
    int numPieces = peerBitVectors[0].size();

    // <pieceNumber, list of peers>
    vector<pair<int, vector<int>>> pieceToPeersMapping;

    for (int pieceNumber = 0; pieceNumber < numPieces; pieceNumber++) {
        vector<int> peersContainingThisPiece;

        for (int peerId = 0; peerId < numPeers; peerId++) {
            string bitVector = peerBitVectors[peerId];
            if (bitVector[pieceNumber] == '1') {
                // TODO: Push back piece number in order of piece download priority
                peersContainingThisPiece.push_back(peerId);
            }
        }
        pieceToPeersMapping.push_back({pieceNumber, peersContainingThisPiece});
    }

    cout << "pieceToPeersMapping before sort: " << endl;
    for (auto e: pieceToPeersMapping) {
        cout << e.first << " -> ";
        for (auto i: e.second)
            cout << i << " ";
        cout << endl;
    }

    sort(pieceToPeersMapping.begin(), pieceToPeersMapping.end(), rarestFirstCmp);

    cout << "pieceToPeersMapping after sort: " << endl;
    for (auto e: pieceToPeersMapping) {
        cout << e.first << " -> ";
        for (auto i: e.second)
            cout << i << " ";
        cout << endl;
    }

    // rarest piece is at the front of list
    for (auto e:pieceToPeersMapping) {
        int pieceNumber = e.first;
        vector<int> peers = e.second;
        int idx = getRandom(0, (int) peers.size() - 1);// generate in range [0, peers.size)
        assert(idx < (int) peers.size());
        assert(peers[idx] < (int) peerToPiecesMap.size());
        peerToPiecesMap[peers[idx]].push_back(pieceNumber);
    }
}

vector<string> getPeerBitVectors(vector<string> &peerList, string shaOfSha, int numPieces) {
    // Initialize empty peerBitVectors
    vector<string> peerBitVectors(peerList.size(), string(numPieces, '0'));
    for (auto i = 0; i < (int) peerBitVectors.size(); i++) {
        cout << "peerBitVectors of " << peerList[i] << " before: " << peerBitVectors[i] << endl;
    }

    // Contact each peer on separate threads
    thread *peerPieceInfoThreads = new thread[peerList.size()];
    for (int i = 0; i < (int) peerList.size(); i++) {
        peerPieceInfoThreads[i] = thread(downloadPeerPieceInfo, peerList[i], shaOfSha, ref(peerBitVectors[i]));
    }

    for (int i = 0; i < (int) peerList.size(); i++) {
        peerPieceInfoThreads[i].join();
    }
    delete[] peerPieceInfoThreads;

    for (auto i = 0; i < (int) peerBitVectors.size(); i++) {
        cout << "peerBitVectors of " << peerList[i] << " after: " << peerBitVectors[i] << endl;
    }

    return peerBitVectors;
}

// Each thread belong to a single peer
// Downloads all pieces to get from this peer
void pieceDownloader(string fileNameToSave,
                     string peerSocketAddress,
                     vector<int> piecesToGet,
                     string shaOfSha,
                     string fileSize) {
    int outfile = open(fileNameToSave.c_str(),
                       O_RDWR | O_CREAT, 0777);

    cout << "Downloading pieces from peer: " << peerSocketAddress << endl;

    for (auto pieceId:piecesToGet) {
        cout << "\t Downloading piece " << pieceId << " from peer: " << peerSocketAddress << endl;

        int socketFD;
        struct sockaddr_in serverAddr;

        socketFD = socket(AF_INET, SOCK_STREAM, 0);

        if (socketFD < 0) {
            cout << "Error in creating Socket for" << peerSocketAddress << endl;
            exit(1);
        }

        vector<string> tokens = getTokens(peerSocketAddress, ':');
        string ip = tokens[0], port = tokens[1];

        memset(&serverAddr, '\0', sizeof(serverAddr));
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(stoi(port));
        serverAddr.sin_addr.s_addr = inet_addr(ip.c_str());

        if (connect(socketFD, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
            cout << "Connect error for " << peerSocketAddress << endl;
            exit(1);
        }

        int requestType = REQUEST_DATA;
        send(socketFD, &requestType, sizeof(int), 0);
        send(socketFD, shaOfSha.c_str(), shaOfSha.length(), 0);
        // Send piece number of the file requested
        send(socketFD, &pieceId, sizeof(pieceId), 0);

        int numChunksToReceive = -1;
        recv(socketFD, &numChunksToReceive, sizeof(numChunksToReceive), 0);

        int currentPieceOffset = pieceId * PIECE_SIZE;
        // Seek to beginning of current piece
        lseek(outfile, currentPieceOffset, SEEK_SET);

        // loop to receive each chunk
        for (int chunkid = 0; chunkid < numChunksToReceive; chunkid++) {
            cout << "\t\t Downloading chunk: " << chunkid << " of piece: " << pieceId << endl;

            int chunkSize = -1;
            recv(socketFD, &chunkSize, sizeof(chunkSize), 0);

            // FIXME: Use Byte buffer instead of char buffer
            char chunkBuffer[chunkSize + 1];
            memset(chunkBuffer, '\0', chunkSize + 1);

            recv(socketFD, chunkBuffer, chunkSize, 0);

            cout << "\t\t Received chunk: " << chunkid << " of piece: " << pieceId << endl;
            cout << "\t\t chunk: " << chunkid << " size: " << chunkSize << endl;
//            cout << "\t\t chunk: " << chunkid << " data: " << chunkBuffer << endl;

            write(outfile, chunkBuffer, chunkSize);

            cout << "\t\t Written chunk: " << chunkid << " of piece: " << pieceId << " to file" << endl;

            // Send for acknowledgement of this chunk
            int ack = 1;
            send(socketFD, &ack, sizeof(ack), 0);
            cout << "\t\t Ack sent." << endl;
        }
        close(socketFD);
        hashPieces[shaOfSha][pieceId] = '1';
    }
    close(outfile);
}

void downloadFile(string mTorrentFilePath, string fileNameToSave) {
    cout << "Here2 downloadFile" << endl;

    vector<string> trackers;
    string hash; // needed to verify each piece of data to be received
    string fileSize;
    fillFieldsFromMtorrentFile(mTorrentFilePath,
                               trackers,
                               fileSize,
                               hash);

    // needed to identify peer list on
    string shaOfSha = getSHAofSHA(hash);
    cout << "getting shaOfSha: " << shaOfSha << endl;

    // Step 1: Get seedersAndLeechers list
    string seedersString = getSeedersFromTracker(shaOfSha, trackers);
    vector<string> peerList = getTokens(seedersString, FIELD_SEPARATOR);

    cout << "List of seeders: " << endl;
    for (auto i:peerList)
        cout << "\t" << i << endl;

    // Step 2: I am now a leecher for this file
    // so notify tracker that I can serve this file in future
    updateSeederLeecherStatusOnTracker(MSG_ADD, shaOfSha, trackers);

    int numPieces = ceil(stof(fileSize) / (float) PIECE_SIZE);
    // bitvector of all zeroes coz I am yet to begin download
    string bitvector(numPieces, '0');
    hashPieces[shaOfSha] = bitvector;
    hashPath[shaOfSha] = fileNameToSave;

    // TODO: Step 3: Get file from peers
    //  - Contact each peer on separate threads to get who contains what piece of the file
    //  - PieceSelectionAlgo: Decide which piece to fetch from whom
    //  - Simultaneously download pieces from different peers
    //  - Update bitvector when a piece download completes
    //  - Retry failed pieces from different peer.


    // Step 3.1: Contact each peer on separate threads to get who contains what piece of the file
    vector<string> peerBitVectors = getPeerBitVectors(peerList, shaOfSha, numPieces);

    // Step 3.2: PieceSelectionAlgo: Decide which piece to fetch from whom
    vector<vector<int>> peerPiecesToGet(peerList.size(), vector<int>());

    pieceSelectionAlgo(peerList, peerBitVectors, peerPiecesToGet);

    for (int peerId = 0; peerId < (int) peerPiecesToGet.size(); peerId++) {
        cout << "Pieces to be fetched from peer " << peerId << " :";
        for (auto e: peerPiecesToGet[peerId])
            cout << e << " ";
        cout << endl;
    }


    thread *pieceDownloaderThreads = new thread[peerList.size()];
    for (int i = 0; i < (int) peerList.size(); i++) {
        pieceDownloaderThreads[i] = thread(pieceDownloader,
                                           fileNameToSave,
                                           peerList[i],
                                           peerPiecesToGet[i],
                                           shaOfSha,
                                           fileSize);

    }
    for (int i = 0; i < (int) peerList.size(); i++) {
        pieceDownloaderThreads[i].join();
    }
    delete[] pieceDownloaderThreads;

    cout << "Downloaded file: " << fileNameToSave << endl;
}

// Creates torrent file for `filePath` and populates `hash` and `trackers`.
void createTorrentFile(string filePath, string mTorrentFileName,
                       string &hash, vector<string> &trackers) {
    hash = getHash(filePath);
    int fileSize = getFileSize(filePath);

    ofstream mTorrentFile(mTorrentFileName, ios_base::ate);

    mTorrentFile << getFileNameFromPath(filePath) << endl;
    mTorrentFile << fileSize << endl;
    mTorrentFile << hash << endl;
    while (true) {
        string trackerSocket;
        cout << "Enter tracker's socket address default(" << defaultTracker << ")." << endl
             << "type " << QUIT << " to quit: ";
        getline(cin, trackerSocket);

        if (trackerSocket.empty())
            trackerSocket = defaultTracker;
        else if (trackerSocket == QUIT)
            break;

        mTorrentFile << trackerSocket << endl;
        trackers.push_back(trackerSocket);
    }

    mTorrentFile.close();
}


void updateSeederLeecherStatusOnTracker(string msgType,
                                        string shaOfSha,
                                        vector<string> &trackers) {
    cout << "Inside updateSeederLeecherStatusOnTracker" << endl;
    int socketFD;
    struct sockaddr_in serverAddress;

    socketFD = socket(AF_INET, SOCK_STREAM, 0);// TODO: switch to UDP
    if (socketFD < 0) {
        cerr << "\tError in creating Socket" << endl;
        exit(1);
    }

    bool connectedToTracker = false;

    for (auto trackerSocketAddr: trackers) {
        vector<string> tokens = getTokens(trackerSocketAddr, ':');
        assert(!tokens.empty());

        memset(&serverAddress, '\0', sizeof(serverAddress));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(stoi(tokens[1]));
        serverAddress.sin_addr.s_addr = inet_addr(tokens[0].c_str());

        cout << "\tConnecting to  tracker: " << trackerSocketAddr << endl;
        if (connect(socketFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
            cout << "\tTracker: " << trackerSocketAddr << " is down." << endl;
        } else {
            connectedToTracker = true;
            break;
        }
    }
    if (!connectedToTracker) {
        cout << "\tAll trackers are down" << endl;
        exit(1);
    }

    stringstream ss;
    ss << msgType << FIELD_SEPARATOR;
    ss << shaOfSha << FIELD_SEPARATOR;
    ss << myIp << FIELD_SEPARATOR;
    ss << myPort << FIELD_SEPARATOR;

    string msg = ss.str();

    int msgLength = msg.length();

    // First send size of the msg to be received by tracker
    send(socketFD, &msgLength, sizeof(msgLength), 0);
    // Send the actual msg
    send(socketFD, msg.c_str(), msgLength, 0);
    cout << "\tStatus updated." << endl;
    close(socketFD);
}

void clientMenu() {
    while (true) {
        string line;
        cout << "Enter command: ";
        getline(cin, line);
        cout << "line: " << line << endl;
        if (line.empty())
            continue; // skip loop

        vector<string> tokens = getTokens(line, ' ');

        if (tokens[0] == CMD_SHARE) {
            // command: share pathOfFileToShare mtorrentFileName
            assert(tokens.size() == 3);

            string hash;
            vector<string> trackers;
            createTorrentFile(tokens[1], tokens[2], hash, trackers);

            string shaOfsha = getSHAofSHA(hash);

            int numPieces = ceil((float) getFileSize(tokens[1]) / (float) PIECE_SIZE);

            string bitvector(numPieces, '1');// bitvector of all ones coz I shared it

            hashPieces[shaOfsha] = bitvector;
            hashPath[shaOfsha] = tokens[1];

            //  Notify tracker that I am serving this file
            updateSeederLeecherStatusOnTracker(MSG_ADD, shaOfsha, trackers);

        } else if (tokens[0] == CMD_GET) {
            cout << "Here2 clientMenu" << endl;
            // command: get mtorrentFilePath fileNameToSave
            assert(tokens.size() == 3);

            // Handle command in separate thread to make asynchronous download possible
            thread downloaderThread(downloadFile, tokens[1], tokens[2]);
            downloaderThread.detach();

        } else {
            cout << "Invalid command" << endl;
            // TODO: write to logs before exiting
            break;
        }
    }
}


int main(int argc, char **argv) {
    auto tokens = getTokens(argv[1], ':');
    myIp = tokens[0];
    myPort = tokens[1];
    defaultTracker = argv[2];

    // NOTE: donot use cin>> anywhere in the code coz things will break
    // use getline(cin, myPort); instead


    // TODO: read/write download log and peer log

    // Fire separate threads for peer server and client menu
    thread serverThread(peerServer);
    thread clientThread(clientMenu);


    // Don't allow main thread to end until both client and server threads are done
    // Joining a thread makes one thread of execution wait for another thread to finish running.
    serverThread.join();
    clientThread.join();

    return 0;
}
