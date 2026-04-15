#pragma once
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <chrono>
#include <thread>
#include <cstring>

void build_rtp_header(uint8_t* buf, uint16_t seq, uint32_t timestamp, uint32_t ssrc, bool first);
void build_rtp_header(uint8_t* buf, uint16_t seq, uint32_t timestamp, uint32_t ssrc); // For send_silence();
void encrypt_payload(const uint8_t* key, uint8_t* iv_base, uint8_t* payload, int len);
void send_silence(AirPlaySession& session, int seconds, bool DEBUG);
void send_audio(AirPlaySession& session, const int16_t* pcm_data, int total_frames);