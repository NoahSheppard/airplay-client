#include <iostream>
#include "../include/discovery.h"

int main() {
    std::cout << "[test_discovery.cpp] Scanning for AirPlay devices" << std::endl;

    auto devices = discover_devices(5000, false);

    if (devices.empty()) {
        std::cout << "[test_discovery.cpp] No devices found." << std::endl;
        return 1;
    }

    for (const auto& dev : devices) {
        std::cout << "\n[test_discovery.cpp]Found: " << dev.name << std::endl;
        std::cout << "[test_discovery.cpp]  IP:         " << dev.ip << std::endl;
        std::cout << "[test_discovery.cpp]  Port:       " << dev.port << std::endl;
        std::cout << "[test_discovery.cpp]  MAC:        " << dev.mac << std::endl;
        std::cout << "[test_discovery.cpp]  Password:   " << (dev.password_required ? "yes" : "no") << std::endl;
    }

    return 0; 
}