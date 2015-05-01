#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <future>
#include <cmath>

#include "MD5.hpp"

using namespace std::chrono;
using namespace std::chrono_literals;

const int gMaxLength = 16;

std::vector<std::string> words;

/* Try each number ([48..57] in ASCII) and lower case letter
 * ([97..122] in ASCII)
 */

uint128_t hash;

/* Converts index in [0..35] to ASCII table range of [48..57] and [97..122]
 * (Numbers and lowercase letters)
 */
unsigned int cvtSearchSpacePosToASCII(unsigned int pos) {
    // Use number range in ASCII table
    if (pos <= 9) {
        return 48 + pos;
    }
    // Else use alphabet later in table
    else {
        return 97 + (pos - 10);
    }
}

// Takes ASCII character as argument
void incSearchSpaceSlot(char& pos) {
    // If at end of numbers 0-9 in ASCII
    if (pos == '9') {
        // Skip to 'a'
        pos = 'a';
    }
    else {
        // Increment normally
        pos++;
    }
}

/* 'beginChar' determines where to start brute-force attack in search space.
 * 'endChar' determines where to stop.
 */
void runBruteforce(unsigned int beginPos, unsigned int endPos) {
    char beginChar = cvtSearchSpacePosToASCII(beginPos);
    char endChar = cvtSearchSpacePosToASCII(endPos);
    unsigned int checkptCount = 0;

    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    time_point<clock> startTime = clock::now();

    std::string password;
    password.reserve(gMaxLength);
    password += beginChar;

    std::string nameStub(1, beginChar);
    nameStub += '-';
    nameStub += endChar;

    // Load last checkpoint
    std::fstream save(nameStub + ".txt", std::fstream::in);
    if (save.is_open()) {
        std::string buffer;
        std::string checkpoint;

        while (std::getline(save, buffer)) {
            checkpoint = buffer;
        }
        save.clear();

        if (checkpoint.length() > 0) {
            std::cout << "Restored from latest password: " << checkpoint <<
                std::endl;
            password = checkpoint;
        }
        else {
            password = beginChar;
        }

        save.close();
    }
    else {
        std::cout << "Failed to open " << nameStub + ".txt" << std::endl;
    }

    // Reopen checkpoint file for writing
    save.open(nameStub + ".txt", std::fstream::out | std::fstream::app);

    // Open password output file
    std::ofstream passwords(nameStub + "-passwds.txt",
                            std::fstream::out | std::ofstream::app);
    if (!passwords.is_open()) {
        std::cout << "Failed to open " << nameStub + "-passwds.txt" <<
            std::endl;
        exit(1);
    }

    std::string endPassword(1, endChar);
    for (unsigned int i = 1; i < password.length(); i++) {
        endPassword += 'z';
    }

    /* Used by thread to determine when searching a segment has been completed.
     * If password starts at 'd', 'd0', 'd00', etc. then endHash should contain
     * 'd', 'dz', 'dzz', etc.
     */
    MD5 md5 = MD5(endPassword);
    uint128_t endHash = md5.getDigest();

    while (password.length() <= gMaxLength) {
        bool overflow = false;
        while (!overflow) {
            MD5 md5 = MD5(password);

            if (md5.getDigest() == hash) {
                std::cout << password << " is a password" << std::endl;

                // Save password
                passwords << password;
                passwords.flush();
            }

            if (md5.getDigest() == endHash) {
                break;
            }

            if (checkptCount == 300000000) {
                static std::mutex checkptMutex;
                std::lock_guard<std::mutex> lock(checkptMutex);

                std::cout << "checkpoint: " << password << std::endl;
                save << password << '\n';
                save.flush();

                checkptCount = 0;
            }
            else {
                checkptCount++;
            }

            unsigned int pos = password.length() - 1;

            incSearchSpaceSlot(password[pos]);

            // If overflow in LSB occurred
            if (password[pos] == 'z' + 1) {
                // Initiate carry of digit
                /* While there are still overflows occurring by carrying digits
                 * and the whole string hasn't overflowed
                 */
                while (password[pos] == 'z' + 1 && !overflow) {
                    /* Carry over the increment to the next place if there is
                     * one
                     */
                    if (pos > 0) {
                        // Set current character to '0'
                        password[pos] = '0';

                        pos--;
                        incSearchSpaceSlot(password[pos]);
                    }
                    else {
                        overflow = true;
                    }
                }
            }
        }

        // Increase string size, then reset string
        password += '0';
        endPassword += 'z';
        MD5 md5 = MD5(endPassword);
        endHash = md5.getDigest();

        // Set all characters in string to start of this thread's partition
        password[0] = beginChar;
        for (unsigned int i = 1; i < password.length(); i++) {
            password[i] = '0';
        }
    }

    std::cout << "elapsed: "
              << duration_cast<milliseconds>(clock::now() - startTime).count() /
        1000.f
              << "s"
              << std::endl;
}

void runDictionary(unsigned int dictBegin, unsigned int dictEnd) {
    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    time_point<clock> startTime = clock::now();
    unsigned int checkptCount = 0;

    std::string numSuffix = "-1";

    std::string nameStub = "dict-";
    nameStub += std::to_string(dictBegin);
    nameStub += '-';
    nameStub += std::to_string(dictEnd);

    // Load last checkpoint
    std::fstream save(nameStub + ".txt", std::fstream::in);
    if (save.is_open()) {
        std::string buffer;
        std::string checkpoint;

        while (std::getline(save, buffer)) {
            checkpoint = buffer;
        }
        save.clear();

        if (checkpoint.length() > 0) {
            std::cout << "Restored from latest password: " << checkpoint <<
                std::endl;
            numSuffix = checkpoint;
        }

        save.close();
    }
    else {
        std::cout << "Failed to open " << nameStub + ".txt" << std::endl;
    }

    // Reopen checkpoint file for writing
    save.open(nameStub + ".txt", std::fstream::out | std::fstream::app);

    // Open password output file
    std::ofstream passwords(nameStub + "-passwds.txt",
                            std::fstream::out | std::ofstream::app);
    if (!passwords.is_open()) {
        std::cout << "Failed to open " << nameStub + "-passwds.txt" <<
            std::endl;
        exit(1);
    }

    for (int currentNum = std::stoi(numSuffix); currentNum <= 1000;
         currentNum++) {
        numSuffix = std::to_string(currentNum);

        for (unsigned int i = dictBegin; i <= dictEnd; i++) {
            if (currentNum != -1) {
                MD5 md5 = MD5(words[i] + numSuffix);
                if (md5.getDigest() == hash) {
                    std::cout << words[i] + numSuffix << " is a password" <<
                        std::endl;

                    // Save password
                    passwords << words[i] + numSuffix;
                    passwords.flush();
                }
            }
            else {
                MD5 md5 = MD5(words[i]);
                if (md5.getDigest() == hash) {
                    std::cout << words[i] << " is a password" << std::endl;

                    // Save password
                    passwords << words[i];
                    passwords.flush();
                }
            }

            if (checkptCount == 1000000) {
                static std::mutex checkptMutex;
                std::lock_guard<std::mutex> lock(checkptMutex);

                std::cout << "checkpoint: " << numSuffix << std::endl;
                save << numSuffix << '\n';
                save.flush();

                checkptCount = 0;
            }
            else {
                checkptCount++;
            }
        }
    }

    std::cout << "elapsed: "
              << duration_cast<milliseconds>(clock::now() - startTime).count() /
        1000.f
              << "s"
              << std::endl;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc + !argc);

    if (args.size() < 2) {
        std::cout << "usage: Hackintosh-cxx (brute|dict) <WU> [<WU>...]\n"
                     "There are 10 possible work units (0..9 inclusive). Pass"
                     "a space delimited list of the ones to run." << std::endl;
        return 0;
    }

    if (args[0] == "dict") {
        std::ifstream dict("dictionary.txt");
        if (dict.is_open()) {
            std::string buffer;
            words.reserve(349900);
            while (std::getline(dict, buffer)) {
                words.push_back(buffer);
            }
        }
        else {
            std::cout << "Failed to open dictionary." << std::endl;
            return 0;
        }
    }

    // std::string hashStr = "a7a189951821c2ebf7bf3167ec3f9fbe";
    // std::string hashStr = "5f4dcc3b5aa765d61d8327deb882cf99";
    // std::string hashStr = "3b11bfdbd675feb0297894dac03a8c04";
    std::string hashStr = "97d3b89397f99594a4981fc6b0cb31b0";
    uint8_t* temp = reinterpret_cast<uint8_t*>(&hash);
    for (unsigned int i = 0; i < 16; i++) {
        temp[i] = std::stoi(hashStr.substr(2 * i, 2), 0, 16);
    }

    std::vector<std::future<void>> threads;
    unsigned int threadCount = 10;

    // Spawn worker threads
    for (unsigned int i = 1; i < args.size(); i++) {
        if (args[0] == "brute") {
            unsigned int beginPos = std::round(std::stoi(args[i]) *
                                               (36.0 / threadCount));
            unsigned int endPos = std::round((std::stoi(args[i]) + 1) *
                                             (36.0 / threadCount)) - 1;

            threads.emplace_back(std::async(std::launch::async, runBruteforce,
                                            beginPos, endPos));
        }
        else if (args[0] == "dict") {
            unsigned int beginPos = std::floor(std::stoi(args[i]) *
                                               (349900.0 / threadCount));
            unsigned int endPos = std::ceil((std::stoi(args[i]) + 1) *
                                            (349900.0 / threadCount)) - 1;

            threads.emplace_back(std::async(std::launch::async, runDictionary,
                                            beginPos, endPos));
        }
    }

    bool threadsDead = false;
    while (!threadsDead) {
        threadsDead = true;
        for (auto& thread : threads) {
            if (thread.wait_for(0s) == std::future_status::deferred) {
                threadsDead = false;
                break;
            }
        }
    }

    return 0;
}

