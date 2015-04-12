#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <future>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/IpAddress.hpp>

using namespace std::chrono;
using namespace std::chrono_literals;

std::vector<std::string> words;

std::string sendPassword(sf::TcpSocket& socket, std::string pass) {
    socket.send(pass.c_str(), pass.length());

    std::string reply;
    char c = 0;
    size_t received = 0;
    while (c != '\n') {
        sf::Socket::Status status = socket.receive(&c, sizeof(c), received);
        if (status == sf::Socket::Done) {
            if (c != '\n') {
                reply += c;
            }
        }
    }

    return reply;
}

void run() {
    sf::TcpSocket socket;

    sf::Socket::Status status = socket.connect(sf::IpAddress("127.0.0.1"),
            58999);
    if (status == sf::Socket::Done) {
        std::cout << "Connecting to server..." << std::endl;
    }
    else {
        std::cout << "Run the server first." << std::endl;
        return;
    }

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
                reply = sendPassword(socket, word + '\n');
            }
            else {
                reply = sendPassword(socket,
                        word + std::to_string(currentNum) + '\n');
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
