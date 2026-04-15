#pragma once
#include <string>
#include <vector>
#include <cstdint>

// Reciever
struct AirPlayDevice {
    std::string name;
    std::string hostname;
    std::string ip;
    uint16_t port;
    std::string mac;

    bool password_required;
    int encryption_types;
    int codec_support;
};

std::vector<AirPlayDevice> discover_devices(int timeout_ms = 5000, bool debug_input = false);