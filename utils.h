#ifndef MUTTOR_UTILS_H
#define MUTTOR_UTILS_H

#include <vector>
#include <string>
#include <sstream>
#include <sys/stat.h>
#include <openssl/sha.h>
#include <fstream>
#include <cmath>
#include <iostream>
#include <cstring>

using namespace std;

#define PIECE_SIZE 524288   // 512*1024 Bytes (512KB)

#define CHUNK_SIZE 2048     // 2*1024 Bytes (2KB)

#define MAX_DATAGRAM_SIZE 2048    // 2*1024 Bytes (2KB)

#define MAX_PENDING_REQUESTS 10

#define LOCALHOST "127.0.0.1"
#define MSG_ADD "ADD"
#define MSG_REMOVE "REMOVE"
#define MSG_SEEDERLIST "SEEDERLIST"

#define FIELD_SEPARATOR '|'

vector<string> getTokens(const string& line, char DELIM);

int getFileSize(const string& filePath);

string getHash(const string& path);

string getFileNameFromPath(const string& path);

string getSHAofSHA(string sha);

string getNodeId(const string& str);

int getRandom(int min, int max);

#endif //MUTTOR_UTILS_H
