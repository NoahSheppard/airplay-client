#pragma once
#include <vector>
#include <cstdint>
#include <string>

struct AirPlaySession {
    int control_socket;
    int audio_socket;
    std::string host;
    int port;
    std::string session_token;
    std::string session_id;
    uint32_t cseq;
    uint8_t aes_key[16];
    uint8_t aes_iv[16];
    uint32_t sequence_number;
    uint32_t timestamp;
    uint16_t server_port;
};

bool rtsp_options(AirPlaySession& session);