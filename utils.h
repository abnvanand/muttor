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

//#define PIECE_SIZE 40000   // STOPSHIP
#define PIECE_SIZE 524288   // 512*1024 Bytes (512KB)

//#define CHUNK_SIZE 6
#define CHUNK_SIZE 2048

#define LOCALHOST "127.0.0.1"
#define MSG_ADD "ADD"
#define MSG_REMOVE "REMOVE"
#define MSG_SEEDERLIST "SEEDERLIST"

#define FIELD_SEPARATOR '|'

vector<string> getTokens(string line, char DELIM);

int getFileSize(string filePath);

string getHash(string path);

string getFileNameFromPath(string path);

string getSHAofSHA(string sha);

#endif //MUTTOR_UTILS_H
