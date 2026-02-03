#include "SentinelGate.hpp"
#include <atomic>
#include <csignal>
#include <chrono>
#include <thread>
#include <iostream>

std::atomic<bool> g_exit{false};

void signal_handler(int) {
    g_exit.store(true);
}

int main() {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    sentinel::SentinelGate gate("192.168.2.200", 1883);
    gate.start("239.255.0.1", 8080, "eth0");

    while (!g_exit.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    gate.stop();
    std::cout << "Exit cleanly\n";
    return 0;
}
