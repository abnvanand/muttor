//
// Created by abnv on 19/11/2019.
//


#include "utils.h"


vector<string> getTokens(string line, char DELIM) {

    string token;
    istringstream ss(line);
    vector<string> res;

    while (getline(ss, token, DELIM)) {
        res.push_back(token);
    }
    return res;
}

int getFileSize(string filePath) {
    struct stat st{};
    if (stat(filePath.c_str(), &st) != 0) {
        return 0;
    }
    return st.st_size;
}

string getHash(string path) {
    // Returns hash for given filepath
    string fileHash = "";

    int size = getFileSize(path) - 1;

    ifstream ifile(path, ios::binary);
    int bytesToRead;

    if (ifile.good()) {
        while (size > 0) {
            bytesToRead = min(size, PIECE_SIZE);

            char buffer[bytesToRead];
            ifile.read(buffer, bytesToRead);

            unsigned char hash[SHA_DIGEST_LENGTH];

            SHA1((unsigned char *) buffer, bytesToRead, hash);

            char hashstr[41];
            for (int i = 0; i < 20; ++i)
                sprintf(&hashstr[i * 2], "%02x", hash[i]);

            hashstr[20] = '\0';// take only first 20 characters of each piece
            string temp(hashstr);
            fileHash = fileHash + temp;

            size -= PIECE_SIZE;
        }
    }
    return fileHash;
}

string getFileNameFromPath(string path) {
    unsigned found = path.find_last_of("/\\");

    clog << "Filepath from path: " << path.substr(0, found) << endl;
    clog << "Filename from path: " << path.substr(found + 1) << endl;

    return path.substr(found + 1);
}

string getSHAofSHA(string sha) {
    int i;
    int size = strlen(&sha[0]);

    unsigned char hash[SHA_DIGEST_LENGTH];

    SHA1((unsigned char *) &sha[0], size, hash);

    char hashstr[41];
    for (i = 0; i < 20; ++i)
        sprintf(&hashstr[i * 2], "%02x", hash[i]);
    hashstr[20] = '\0';
    string SHA(hashstr);

    return SHA;
}


int getRandom(int min, int max) {
//range : [min, max]
    static bool first = true;
    if (first) {
        srand(time(nullptr)); //seeding for the first time only!
        first = false;
    }
    return min + rand() % ((max + 1) - min);
}

