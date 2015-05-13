#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <future>
#include <algorithm>
#include <cmath>

#include "MD5.hpp"

using namespace std::chrono;
using namespace std::chrono_literals;

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
void runBruteforce(unsigned int beginPos, unsigned int endPos, int maximum) {
    char beginChar = cvtSearchSpacePosToASCII(beginPos);
    char endChar = cvtSearchSpacePosToASCII(endPos);
    unsigned int checkptCount = 0;

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

    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    time_point<clock> startTime = clock::now();

    while (password.length() <= (unsigned int) maximum) {
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
        for (unsigned int i = 1; i < password.length() - 1; i++) {
            password[i] = '0';
        }
    }

    std::cout << "elapsed: "
              << duration_cast<milliseconds>(clock::now() - startTime).count() /
        1000.f
              << "s"
              << std::endl;
}

void runDictionary(unsigned int dictBegin, unsigned int dictEnd, int maximum) {
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

    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    time_point<clock> startTime = clock::now();

    for (int currentNum = std::stoi(numSuffix); currentNum <= maximum;
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

            if (checkptCount == 30000000) {
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

void runCombo(unsigned int dictBegin, unsigned int dictEnd, int maximum) {
    unsigned int checkptCount = 0;

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
                      << words[std::stoi(checkpoint)]
                      << std::endl;
            dictBegin = std::stoi(checkpoint);
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

    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    time_point<clock> startTime = clock::now();

    std::string numStr;
    for (unsigned int i = dictBegin; i <= dictEnd; i++) {
        for (unsigned int j = 0; j < words.size(); j++) {
            numStr.clear();
            while (numStr.length() <= 3) {
                if (numStr.length() != 0) {
                    MD5 md5 = MD5(words[i] + words[j] + numStr);
                    if (md5.getDigest() == hash) {
                        std::cout << words[i] + words[j] + numStr
                                  << " is a password" << std::endl;

                        // Save password
                        passwords << words[i] + words[j] + numStr;
                        passwords.flush();
                    }

                    unsigned int pos = numStr.length() - 1;

                    // Increment right-most character
                    numStr[pos]++;

                    /* While there are overflows occurring by carrying digits and the
                     * whole string hasn't overflowed.
                     *
                     * The contents of this while loop would crash at
                     * "incSearchSpaceSlot(password[pos]);" if "password" were a
                     * one-character string, but comparing against endPassword is
                     * sufficient to terminate the "while (!overflow)" loop before that
                     * can happen.
                     */
                    while (numStr[pos] == '9' + 1) {
                        /* Carries are occurring. If there is no place to which to
                         * carry, an overflow occurred.
                         */
                        if (pos == 0) {
                            // Increase string size, then reset string
                            numStr += '0';

                            // Set all characters in string to start of this thread's partition
                            for (unsigned int i = 0;
                                 i < numStr.length() - 1;
                                 i++) {
                                numStr[i] = '0';
                            }
                            break;
                        }

                        // Reset current character to '0'.
                        numStr[pos] = '0';

                        // Carry over the increment to the next place
                        pos--;
                        numStr[pos]++;
                    }
                }
                else {
                    MD5 md5 = MD5(words[i] + words[j]);
                    if (md5.getDigest() == hash) {
                        std::cout << words[i] + words[j]
                                  << " is a password" << std::endl;

                        // Save password
                        passwords << words[i] + words[j];
                        passwords.flush();
                    }

                    numStr += '0';
                }

                if (checkptCount == 60000000) {
                    static std::mutex checkptMutex;
                    std::lock_guard<std::mutex> lock(checkptMutex);

                    std::cout << "checkpoint: " << words[i] << std::endl;
                    save << i << '\n';
                    save.flush();

                    checkptCount = 0;
                }
                else {
                    checkptCount++;
                }
            }

            // Increase string size, then reset string
            numStr += '0';

            // Set all characters in string to start of this thread's partition
            for (unsigned int i = 0; i < numStr.length() - 1; i++) {
                numStr[i] = '0';
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
    unsigned int threadCount = 20;

    if (!(args.size() >= 2 && (args[0] == "brute" || args[0] == "dict" ||
                               args[0] == "combo"))) {
        std::cout << "usage: Hackintosh-cxx (brute|dict|combo) (<WU> [<WU>...]"
                     "|benchmark)\n"
                     "There are " << threadCount << " possible work units "
                  << "(0.." << threadCount - 1 << " inclusive). Pass a space"
                                        "delimited list of the ones to run." <<
        std::endl;
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
    else if (args[0] == "combo") {
        std::ifstream dict("dictionary.txt");
        if (dict.is_open()) {
            std::string buffer;
            words.reserve(349900);
            words.push_back("");
            while (std::getline(dict, buffer)) {
                words.push_back(buffer);
            }

            std::sort(words.begin(), words.end(), [] (auto& i, auto& j) {
                return i.length() < j.length();
            });
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

    if (args[1] == "benchmark") {
        if (args[0] == "brute") {
            threads.emplace_back(std::async(std::launch::async, runBruteforce,
                                            0, 3, 6));
        }
        else if (args[0] == "dict") {
            threads.emplace_back(std::async(std::launch::async,
                                            runDictionary, 0, words.size() - 1,
                                            2500));
        }
        else if (args[0] == "combo") {
            threads.emplace_back(std::async(std::launch::async,
                                            runCombo, 0, 1, 100));
        }
    }
    else {
        // Spawn worker threads
        for (unsigned int i = 1; i < args.size(); i++) {
            if (args[0] == "brute") {
                unsigned int beginPos = std::round(std::stoi(args[i]) *
                                                   (36.0 / threadCount));
                unsigned int endPos = std::round((std::stoi(args[i]) + 1) *
                                                 (36.0 / threadCount)) - 1;

                threads.emplace_back(std::async(std::launch::async,
                                                runBruteforce, beginPos, endPos,
                                                16));
            }
            else if (args[0] == "dict") {
                unsigned int beginPos = std::floor(std::stoi(args[i]) *
                                                   (static_cast<float>(words.
                                                                       size()) /
                                                    threadCount));
                unsigned int endPos = std::ceil((std::stoi(args[i]) + 1) *
                                                (static_cast<float>(words.size())
                                                 / threadCount)) - 1;

                threads.emplace_back(std::async(std::launch::async,
                                                runDictionary, beginPos, endPos,
                                                999999999));
            }
            else if (args[0] == "combo") {
                unsigned int beginPos = std::floor(std::stoi(args[i]) *
                                                   (static_cast<float>(words.
                                                                       size()) /
                                                    threadCount));
                unsigned int endPos = std::ceil((std::stoi(args[i]) + 1) *
                                                (static_cast<float>(words.size())
                                                 / threadCount)) - 1;

                threads.emplace_back(std::async(std::launch::async,
                                                runCombo, beginPos, endPos,
                                                1000));
            }
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

