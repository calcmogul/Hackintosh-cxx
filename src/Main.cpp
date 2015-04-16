#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <future>
#include <atomic>
#include <cmath>

#include "MD5.hpp"

using namespace std::chrono;
using namespace std::chrono_literals;

const int gMaxLength = 16;
std::atomic<char> passwordsFound{0};

std::vector<std::string> words;

/* Try each number ([48..57] in ASCII) and lower case letter
 * ([97..122] in ASCII)
 */

uint8_t hash[16];

// Return value of true means a password matched
bool sendPassword(std::string pass) {
    MD5 md5 = MD5(pass);

    if (*reinterpret_cast<__uint128_t*>(md5.getDigest()) ==
        *reinterpret_cast<__uint128_t*>(hash)) {
        return true;
    }
    else {
        return false;
    }
}

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
char incSearchSpaceSlot(char pos) {
    // If at end of numbers 0-9 in ASCII
    if (pos == '9') {
        // Skip to 'a'
        return 'a';
    }
    else {
        // Increment normally
        return pos + 1;
    }
}

/* 'beginChar' determines where to start brute-force attack in search space.
 * 'endChar' determines where to stop.
 */
void runBruteforce(char beginChar, char endChar) {
    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    time_point<clock> currentTime = clock::now();
    time_point<clock> lastTime = currentTime;

    std::string password;
    password.reserve(gMaxLength);

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

        if (checkpoint.size() > 0) {
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

    std::string endPassword(password.length(), endChar);

    std::cout << "Started computation" << std::endl;

    while (password.length() <= gMaxLength) {
        bool overflow = false;
        while (!overflow) {
            if (password == endPassword) {
                break;
            }

            if (passwordsFound == 4) {
                return;
            }

            if (sendPassword(password)) {
                std::cout << password << " is a password" << std::endl;
                passwordsFound++;

                // Save password
                passwords << password;
                passwords.flush();
            }

            currentTime = clock::now();
            if (currentTime - lastTime > 1min) {
                std::cout << "checkpoint: " << password << std::endl;
                save << password << '\n';
                save.flush();

                lastTime = currentTime;
            }

            unsigned int lsb = password.length() - 1;

            password[lsb] = incSearchSpaceSlot(password[lsb]);

            // If overflow in LSB occurred
            if (password[lsb] == 'z' + 1) {
                // Initiate carry of digit
                int carry = lsb;
                /* While there are still overflows occurring by carrying digits
                 * and the whole string hasn't overflowed
                 */
                while (!overflow && password[carry] == 'z' + 1) {
                    // Set current character to '0'
                    password[carry] = '0';

                    /* Carry over the increment to the next place if there is
                     * one
                     */
                    if (carry > 0) {
                        carry--;
                        password[carry] = incSearchSpaceSlot(password[carry]);
                    }
                    else {
                        overflow = true;
                    }
                }
            }
        }

        // Increase string size, then reset string
        password += beginChar;
        endPassword += endChar;

        // Set all characters in string to start of this thread's partition
        for (auto& c : password) {
            c = beginChar;
        }
    }
}

void runDictionary(unsigned int dictBegin, unsigned int dictEnd) {
    // Prepare timing for checkpoints
    using clock = std::chrono::system_clock;
    time_point<clock> currentTime = clock::now();
    time_point<clock> lastTime = currentTime;

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

        if (checkpoint.size() > 0) {
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
            if (passwordsFound == 4) {
                return;
            }

            if (currentNum != -1) {
                if (sendPassword(words[i] + numSuffix)) {
                    std::cout << words[i] + numSuffix << " is a password" <<
                        std::endl;
                    passwordsFound++;

                    // Save password
                    passwords << words[i] + numSuffix;
                    passwords.flush();
                }
            }
            else {
                if (sendPassword(words[i])) {
                    std::cout << words[i] << " is a password" << std::endl;
                    passwordsFound++;

                    // Save password
                    passwords << words[i];
                    passwords.flush();
                }
            }

            currentTime = clock::now();
            if (currentTime - lastTime > 1min) {
                std::cout << "checkpoint: " << numSuffix << std::endl;
                save << numSuffix << '\n';
                save.flush();

                lastTime = currentTime;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc + !argc);

    if (args.size() == 0) {
        std::cout << "There are 10 possible work units (0..9 inclusive). Pass"
                     "a space delimited list of the ones to run." << std::endl;
        return 0;
    }

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

    // std::string hashStr = "a7a189951821c2ebf7bf3167ec3f9fbe";
    // std::string hashStr = "5f4dcc3b5aa765d61d8327deb882cf99";
    // std::string hashStr = "3b11bfdbd675feb0297894dac03a8c04";
    std::string hashStr = "97d3b89397f99594a4981fc6b0cb31b0";
    for (unsigned int i = 0; i < 16; i++) {
        hash[i] = std::stoi(hashStr.substr(2 * i, 2), 0, 16);
    }

    std::vector<std::future<void>> threads;
    unsigned int threadCount = 10;

    // Spawn worker threads
    for (const auto& arg : args) {
        /*unsigned int beginPos = std::round(std::stoi(arg) *
         *       (36.0 / threadCount));
         *  unsigned int endPos = std::round((std::stoi(arg) + 1) *
         *       (36.0 / threadCount)) - 1;
         *
         *  threads.emplace_back(std::async(std::launch::async, runBruteforce,
         *       cvtSearchSpacePosToASCII(beginPos),
         *       cvtSearchSpacePosToASCII(endPos)));*/

        unsigned int beginPos = std::round(std::stoi(arg) *
                                           (349900.0 / threadCount));
        unsigned int endPos = std::round((std::stoi(arg) + 1) *
                                         (349900.0 / threadCount)) - 1;

        threads.emplace_back(std::async(std::launch::async, runDictionary,
                                        beginPos, endPos));
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

