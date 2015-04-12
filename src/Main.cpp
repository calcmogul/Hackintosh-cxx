#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <future>

#include "md5.h"

using namespace std::chrono;
using namespace std::chrono_literals;

std::vector<std::string> words;

std::string sendPassword(std::string pass) {
    std::string hash = md5(pass);

    if (hash == "a7a189951821c2ebf7bf3167ec3f9fbe" ||
        hash == "5f4dcc3b5aa765d61d8327deb882cf99" ||
        hash == "3b11bfdbd675feb0297894dac03a8c04" ||
        hash == "97d3b89397f99594a4981fc6b0cb31b0") {
        return "yes";
    }
    else {
        return "no";
    }
}

void run() {
    using clock = std::chrono::system_clock;
    time_point<clock> currentTime = clock::now();
    time_point<clock> lastTime = currentTime;
    time_point<clock> lastPrint = currentTime;

    std::string password;
    password.reserve(32);

    std::string reply;

    for (int currentNum = -1; currentNum <= 100; currentNum++) {
        std::cout << "currentNum=" << currentNum << std::endl;
        for (const auto& word : words) {
            password.clear();

            if (currentNum == -1) {
                reply = sendPassword(word);
            }
            else {
                reply = sendPassword(word + std::to_string(currentNum));
            }

            if (reply == "yes") {
                std::cout << word << " is a password" << std::endl;
            }

            currentTime = clock::now();
            if (currentTime - lastPrint > 100000000ns) {
                lastPrint = currentTime;
                std::cout << (currentTime - lastTime).count() / 1000000.0
                        << "ms" << std::endl;
            }
            lastTime = currentTime;
        }
    }
}

int main() {
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

    std::vector<std::future<void>> threads;
    for (unsigned int i = 0; i < 1; i++) {
        //for (unsigned int i = 0; i < std::thread::hardware_concurrency(); i++) {
        threads.emplace_back(std::async(std::launch::async, run));
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
