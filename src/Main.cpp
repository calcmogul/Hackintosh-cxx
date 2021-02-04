// Copyright (c) Tyler Veness 2015-2017. All Rights Reserved.

#include <stdint.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "MD5.hpp"

using namespace std::chrono;

std::vector<std::string> words;

/* Try each number ([48..57] in ASCII) and lower case letter
 * ([97..122] in ASCII)
 */

uint128_t gHash;

/* Converts index in [0..61] to ASCII table range of [48..57], [65..90], and
 * [97..122] (Numbers, uppercase letters, and lowercase letters)
 */
uint32_t cvtSearchSpacePosToASCII(uint32_t pos) {
    if (pos <= 9) {
        // Use number range in ASCII table
        return 48 + pos;
    } else if (pos <= 35) {
        // Use uppercase in ASCII table
        return 65 + (pos - 10);
    } else {
        // Else use alphabet later in table
        return 97 + (pos - 10 - 26);
    }
}

// Takes ASCII character as argument
void incSearchSpaceSlot(char& pos) {
    // If at end of numbers 0-9 in ASCII
    if (pos == '9') {
        // Skip to 'A'
        pos = 'A';
    } else if (pos == 'Z') {
        pos = 'a';
    } else {
        // Increment normally
        pos++;
    }
}

/* 'beginChar' determines where to start brute-force attack in search space.
 * 'endChar' determines where to stop.
 * 'maximum' determines maximum length of passwords to search.
 */
void runBruteforce(uint32_t beginPos, uint32_t endPos, int maximum,
                   int affinity) {
#ifndef _WIN32
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(affinity, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cout << "Failed setting thread affinity" << std::endl;
    }
#endif  // _WIN32

    uint128_t hash = gHash;
    char beginChar = cvtSearchSpacePosToASCII(beginPos);
    char endChar = cvtSearchSpacePosToASCII(endPos);
    uint32_t checkptCount = 0;

    std::string password;
    password.reserve(maximum);
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
            std::cout << "Restored from latest password: " << checkpoint
                      << std::endl;
            password = checkpoint;
        } else {
            password = beginChar;
        }

        save.close();
    } else {
        std::cout << "Failed to open " << nameStub + ".txt" << std::endl;
    }

    // Reopen checkpoint file for writing
    save.open(nameStub + ".txt", std::fstream::out | std::fstream::app);

    // Open password output file
    std::ofstream passwords(nameStub + "-passwds.txt",
                            std::fstream::out | std::ofstream::app);
    if (!passwords.is_open()) {
        std::cout << "Failed to open " << nameStub + "-passwds.txt"
                  << std::endl;
        std::exit(1);
    }

    std::string endPassword(1, endChar);
    for (uint32_t i = 1; i < password.length(); i++) {
        endPassword += 'z';
    }

    /* Used by thread to determine when searching a segment has been completed.
     * If password starts at 'd', 'd0', 'd00', etc. then endHash should contain
     * 'd', 'dz', 'dzz', etc.
     */
    MD5 md5 = MD5(endPassword);
    uint128_t endHash = md5.getDigest();

    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    auto startTime = clock::now();

    while (password.length() <= static_cast<uint32_t>(maximum)) {
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
            } else {
                checkptCount++;
            }

            uint32_t pos = password.length() - 1;

            // Increment right-most character
            incSearchSpaceSlot(password[pos]);

            /* While there are overflows occurring by carrying digits and the
             * whole string hasn't overflowed.
             *
             * The contents of this while loop would crash at
             * "incSearchSpaceSlot(password[pos]);" if "password" were a
             * one-character string, but comparing against endPassword is
             * sufficient to terminate the "while (!overflow)" loop before that
             * can happen.
             */
            while (password[pos] == 'z' + 1) {
                /* Carries are occurring. If there is no place to which to
                 * carry, an overflow occurred.
                 */
                if (pos == 0) {
                    overflow = true;
                    break;
                }

                // Reset current character to '0'.
                password[pos] = '0';

                // Carry over the increment to the next place
                pos--;
                incSearchSpaceSlot(password[pos]);
            }
        }

        // Increase string size, then reset string
        password += '0';
        endPassword += 'z';
        MD5 md5 = MD5(endPassword);
        endHash = md5.getDigest();

        // Set all characters in string to start of this thread's partition
        password[0] = beginChar;
        for (uint32_t i = 1; i < password.length() - 1; i++) {
            password[i] = '0';
        }
    }

    std::cout << "elapsed: "
              << duration_cast<milliseconds>(clock::now() - startTime).count() /
                     1000.f
              << "s" << std::endl;
}

/*
 * 'dictBegin' contains the index of the dictionary entry at which to start.
 * 'dictEnd' contains the index of the dictionary entry at which to stop.
 * 'maximum' determines the maximum length of passwords to search.
 */
void runDictionary(uint32_t dictBegin, uint32_t dictEnd, int maximum,
                   int affinity) {
#ifndef _WIN32
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(affinity, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cout << "Failed setting thread affinity" << std::endl;
    }
#endif  // _WIN32

    uint128_t hash = gHash;
    uint32_t checkptCount = 0;

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
            std::cout << "Restored from latest password: " << checkpoint
                      << std::endl;
            numSuffix = checkpoint;
        }

        save.close();
    } else {
        std::cout << "Failed to open " << nameStub + ".txt" << std::endl;
    }

    // Reopen checkpoint file for writing
    save.open(nameStub + ".txt", std::fstream::out | std::fstream::app);

    // Open password output file
    std::ofstream passwords(nameStub + "-passwds.txt",
                            std::fstream::out | std::ofstream::app);
    if (!passwords.is_open()) {
        std::cout << "Failed to open " << nameStub + "-passwds.txt"
                  << std::endl;
        std::exit(1);
    }

    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    time_point<clock> startTime = clock::now();

    for (int currentNum = std::stoi(numSuffix); currentNum <= maximum;
         currentNum++) {
        numSuffix = std::to_string(currentNum);

        for (uint32_t i = dictBegin; i <= dictEnd; i++) {
            if (currentNum != -1) {
                MD5 md5 = MD5(words[i] + numSuffix);
                if (md5.getDigest() == hash) {
                    std::cout << words[i] + numSuffix << " is a password"
                              << std::endl;

                    // Save password
                    passwords << words[i] + numSuffix;
                    passwords.flush();
                }
            } else {
                MD5 md5 = MD5(words[i]);
                if (md5.getDigest() == hash) {
                    std::cout << words[i] << " is a password" << std::endl;

                    // Save password
                    passwords << words[i];
                    passwords.flush();
                }
            }

            if (checkptCount == 30000000) {
                static std::mutex checkptMutex;
                std::lock_guard<std::mutex> lock(checkptMutex);

                std::cout << "checkpoint: " << numSuffix << std::endl;
                save << numSuffix << '\n';
                save.flush();

                checkptCount = 0;
            } else {
                checkptCount++;
            }
        }
    }

    std::cout << "elapsed: "
              << duration_cast<milliseconds>(clock::now() - startTime).count() /
                     1000.f
              << "s" << std::endl;
}

/*
 * 'dictBegin' contains the index of the dictionary entry at which to start.
 * 'dictEnd' contains the index of the dictionary entry at which to stop.
 * 'maximum' determines the maximum length of passwords to search.
 */
void runCombo(uint32_t dictBegin, uint32_t dictEnd, int maximum, int affinity) {
#ifndef _WIN32
    cpu_set_t cpuset;
    pthread_t thread = pthread_self();

    CPU_ZERO(&cpuset);
    CPU_SET(affinity, &cpuset);
    if (pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset) != 0) {
        std::cout << "Failed setting thread affinity" << std::endl;
    }
#endif  // _WIN32

    uint128_t hash = gHash;
    uint32_t checkptCount = 0;

    std::string nameStub = "combo-";
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
            std::cout << "Restored from latest password: "
                      << words[std::stoi(checkpoint)] << std::endl;
            dictBegin = std::stoi(checkpoint);
        }

        save.close();
    } else {
        std::cout << "Failed to open " << nameStub + ".txt" << std::endl;
    }

    // Reopen checkpoint file for writing
    save.open(nameStub + ".txt", std::fstream::out | std::fstream::app);

    // Open password output file
    std::ofstream passwords(nameStub + "-passwds.txt",
                            std::fstream::out | std::ofstream::app);
    if (!passwords.is_open()) {
        std::cout << "Failed to open " << nameStub + "-passwds.txt"
                  << std::endl;
        std::exit(1);
    }

    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    time_point<clock> startTime = clock::now();

    std::string numStr;
    for (uint32_t i = dictBegin; i <= dictEnd; i++) {
        for (uint32_t j = 0; j < words.size(); j++) {
            // Don't check for possibility that there is no number suffix
            numStr = "0";
            while (numStr.length() <= 2) {
                if (words[i].length() + words[j].length() + numStr.length() >=
                    9) {
                    MD5 md5 = MD5(words[i] + words[j] + numStr);
                    if (md5.getDigest() == hash) {
                        std::cout << words[i] + words[j] + numStr
                                  << " is a password" << std::endl;

                        // Save password
                        passwords << words[i] + words[j] + numStr;
                        passwords.flush();
                    }
                }

                uint32_t pos = numStr.length() - 1;

                // Increment right-most character
                numStr[pos]++;

                /* While there are overflows occurring by carrying digits and
                 * the whole string hasn't overflowed.
                 *
                 * The contents of this while loop would crash at
                 * "incSearchSpaceSlot(password[pos]);" if "password" were a
                 * one-character string, but comparing against endPassword is
                 * sufficient to terminate the "while (!overflow)" loop before
                 * that can happen.
                 */
                while (numStr[pos] == '9' + 1) {
                    /* Carries are occurring. If there is no place to which to
                     * carry, an overflow occurred.
                     */
                    if (pos == 0) {
                        // Increase string size, then reset string
                        numStr += '0';

                        // Set all characters in string to start of this
                        // thread's partition
                        for (uint32_t k = 0; k < numStr.length() - 1; k++) {
                            numStr[k] = '0';
                        }
                        break;
                    }

                    // Reset current character to '0'.
                    numStr[pos] = '0';

                    // Carry over the increment to the next place
                    pos--;
                    numStr[pos]++;
                }

                if (checkptCount == 60000000) {
                    static std::mutex checkptMutex;
                    std::lock_guard<std::mutex> lock(checkptMutex);

                    std::cout << "checkpoint: " << words[i] << std::endl;
                    save << i << '\n';
                    save.flush();

                    checkptCount = 0;
                } else {
                    checkptCount++;
                }
            }

            // Increase string size, then reset string
            numStr += '0';

            // Set all characters in string to start of this thread's partition
            for (uint32_t i = 0; i < numStr.length() - 1; i++) {
                numStr[i] = '0';
            }
        }
    }

    std::cout << "elapsed: "
              << duration_cast<milliseconds>(clock::now() - startTime).count() /
                     1000.f
              << "s" << std::endl;
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc + !argc);
    constexpr uint32_t kNumThreads = 20;
    constexpr double kNumSymbols = 36.0;

    if (!(args.size() >= 2 &&
          (args[0] == "brute" || args[0] == "dict" || args[0] == "combo"))) {
        std::cout << "usage: Hackintosh-cxx (brute|dict|combo) (<WU> [<WU>...]"
                     "|benchmark(#))\n"
                     "There are "
                  << kNumThreads << " possible work units "
                                    "(0.."
                  << kNumThreads - 1
                  << " inclusive). Pass a space delimited list of the ones to "
                     "run. Append a number to the benchmark command to run it "
                     "in that many threads. Valid numbers are 0 through 9 "
                     "inclusive."
                  << std::endl;
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
        } else {
            std::cout << "Failed to open dictionary." << std::endl;
            return 0;
        }
    } else if (args[0] == "combo") {
        std::ifstream dict("dictionary.txt");
        if (dict.is_open()) {
            std::string buffer;
            words.reserve(349900);
            words.push_back("");
            while (std::getline(dict, buffer)) {
                words.push_back(buffer);
            }

            std::sort(words.begin(), words.end(),
                      [](auto& i, auto& j) { return i.length() < j.length(); });
        } else {
            std::cout << "Failed to open dictionary." << std::endl;
            return 0;
        }
    }

    // std::string hashStr = "a7a189951821c2ebf7bf3167ec3f9fbe";
    // std::string hashStr = "5f4dcc3b5aa765d61d8327deb882cf99";
    // std::string hashStr = "3b11bfdbd675feb0297894dac03a8c04";
    std::string hashStr = "97d3b89397f99594a4981fc6b0cb31b0";
    auto temp = reinterpret_cast<uint8_t*>(&gHash);
    for (uint32_t i = 0; i < 16; i++) {
        temp[i] = std::stoi(hashStr.substr(2 * i, 2), 0, 16);
    }

    std::vector<std::thread> threads;

    // "brute" attack
    if (args[0] == "brute") {
        if (args[1].substr(0, 9) == "benchmark" && args[1].length() > 9) {
            uint8_t threadMax = args[1][9] - '0';
            for (uint32_t i = 0; i < threadMax; i++) {
                threads.emplace_back(runBruteforce, 4 * i, 4 * i + 3, 6, i);
            }
        } else {
            // Spawn worker threads
            for (uint32_t i = 1; i < args.size(); i++) {
                uint32_t beginPos =
                    std::round(std::stoi(args[i]) * (kNumSymbols / kNumThreads));
                uint32_t endPos = std::round((std::stoi(args[i]) + 1) *
                                             (kNumSymbols / kNumThreads)) -
                                  1;

                threads.emplace_back(runBruteforce, beginPos, endPos, 16, i);
            }
        }
    } else if (args[0] == "dict") {
        // "dict" attack

        if (args[1].substr(0, 9) == "benchmark" && args[1].length() > 9) {
            uint8_t threadMax = args[1][9] - '0';
            for (uint32_t i = 0; i < threadMax; i++) {
                threads.emplace_back(runDictionary, 0, words.size() - 1, 2500,
                                     i);
            }
        } else {
            // Spawn worker threads
            for (uint32_t i = 1; i < args.size(); i++) {
                uint32_t beginPos = std::floor(
                    std::stoi(args[i]) *
                    (static_cast<float>(words.size()) / kNumThreads));
                uint32_t endPos = std::ceil((std::stoi(args[i]) + 1) *
                                            (static_cast<float>(words.size()) /
                                             kNumThreads)) -
                                  1;

                threads.emplace_back(runDictionary, beginPos, endPos, 999999999,
                                     i);
            }
        }
    } else if (args[0] == "combo") {
        // "combo" attack

        if (args[1].substr(0, 9) == "benchmark" && args[1].length() > 9) {
            uint8_t threadMax = args[1][9] - '0';
            for (int i = 0; i < threadMax; i++) {
                threads.emplace_back(runCombo, 4 * i, 4 * i + 3, 100, i);
            }
        } else {
            // Spawn worker threads
            for (uint32_t i = 1; i < args.size(); i++) {
                uint32_t beginPos = std::floor(
                    std::stoi(args[i]) *
                    (static_cast<float>(words.size()) / kNumThreads));
                uint32_t endPos = std::ceil((std::stoi(args[i]) + 1) *
                                            (static_cast<float>(words.size()) /
                                             kNumThreads)) -
                                  1;

                threads.emplace_back(runCombo, beginPos, endPos, 1000, i);
            }
        }
    }

    for (auto& thread : threads) {
        thread.join();
    }

    return 0;
}
