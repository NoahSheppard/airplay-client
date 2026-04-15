#include "../include/rtsp_client.h"
#include "../include/rtp_sender.h"
#include "../include/alac_encoder.h"
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <chrono>
#include <thread>
#include <cstring>
#include <sys/socket.h>

static const int FRAMES_PER_PACKET = 352;
static const int PAYLOAD_SIZE = 480; // 352*2ch * 16bit rounded up to AES block

void build_rtp_header(uint8_t* buf, uint16_t seq, uint32_t timestamp, uint32_t ssrc, bool first) {
    buf[0] = 0x80; // V = 2, P = 0, X = 0, CC = 0, M - 9, PT = 96 (dynamic, ALAC)
    buf[1] = first ? 0xE0 : 0x60;
    buf[2] = (seq >> 8) & 0xFF;
    buf[3] = seq & 0xFF;
    buf[4] = (timestamp >> 24) & 0xFF;
    buf[5] = (timestamp >> 16) & 0xFF;
    buf[6] = (timestamp >> 8) & 0xFF;
    buf[7] = timestamp & 0xFF;
    buf[8] = (ssrc >> 24) & 0xFF;
    buf[9] = (ssrc >> 16) & 0xFF;
    buf[10] = (ssrc >> 8) & 0xFF;
    buf[11] = ssrc & 0xFF;
}

void build_rtp_header(uint8_t* buf, uint16_t seq, uint32_t timestamp, uint32_t ssrc) {
    buf[0] = 0x80; // V = 2, P = 0, X = 0, CC = 0, M - 9, PT = 96 (dynamic, ALAC)
    buf[1] = 0x60;
    buf[2] = (seq >> 8) & 0xFF;
    buf[3] = seq & 0xFF;
    buf[4] = (timestamp >> 24) & 0xFF;
    buf[5] = (timestamp >> 16) & 0xFF;
    buf[6] = (timestamp >> 8) & 0xFF;
    buf[7] = timestamp & 0xFF;
    buf[8] = (ssrc >> 24) & 0xFF;
    buf[9] = (ssrc >> 16) & 0xFF;
    buf[10] = (ssrc >> 8) & 0xFF;
    buf[11] = ssrc & 0xFF;
}

void encrypt_payload(const uint8_t* key, uint8_t* iv_base, uint8_t* payload, int len) {
    uint8_t iv[16];
    memcpy(iv, iv_base, 16);

    AES_KEY aes;
    AES_set_encrypt_key(key, 128, &aes);

    int blocks = (len / 16) * 16;
    AES_cbc_encrypt(payload, payload, blocks, &aes, iv, AES_ENCRYPT);
}

void send_silence(AirPlaySession& session, int seconds, bool DEBUG) {
    uint32_t ssrc;
    RAND_bytes((uint8_t*)&ssrc, 4);

    const int HEADER = 12;
    const int PKT_LEN = HEADER + PAYLOAD_SIZE;
    uint8_t packet[PKT_LEN];

    const auto interval = std::chrono::microseconds(352 * 1000000 / 44100);
    int total_packets = (seconds * 44100) / FRAMES_PER_PACKET;

    for (int i = 0; i < total_packets; i++) {
        memset(packet, 0, PKT_LEN);
        build_rtp_header(packet, session.sequence_number, session.timestamp, ssrc);

        uint8_t* payload = packet + HEADER;
        encrypt_payload(session.aes_key, session.aes_iv, payload, PAYLOAD_SIZE);

        sendto(
            session.audio_socket, 
            packet,
            PKT_LEN, 
            0, 
            (struct sockaddr*)&session.rtp_dest,
            sizeof(session.rtp_dest) 
        );

        session.sequence_number++;
        session.timestamp += FRAMES_PER_PACKET;

        std::this_thread::sleep_for(interval);
    }
}

void send_audio(AirPlaySession& session, const int16_t* pcm_data, int total_frames) {
    ALACEncoder* enc = make_alac_encoder();

    uint32_t ssrc;
    RAND_bytes((uint8_t*)&ssrc, 4);

    const auto interval = std::chrono::microseconds(FRAMES_PER_PACKET * 1000000LL / 44100);

    uint8_t alac_buf[2048];
    uint8_t packet[2048 + 12];

    int frame_offset = 0;
    bool first = true;

    while (frame_offset + FRAMES_PER_PACKET <= total_frames) {
        const int16_t* chunk = pcm_data + frame_offset * 2;

        int alac_len = encode_alac_frame(enc, chunk, alac_buf, sizeof(alac_buf));

        uint8_t payload[2048];
        memcpy(payload, alac_buf, alac_len);
        encrypt_payload(session.aes_key, session.aes_iv, payload, alac_len);

        build_rtp_header(packet, session.sequence_number, session.timestamp, ssrc, first);
        memcpy(packet+12, payload, alac_len);

        sendto(session.audio_socket, packet, 12 + alac_len, 0, (struct sockaddr*)&session.rtp_dest, sizeof(session.rtp_dest));

        session.sequence_number++;
        session.timestamp += FRAMES_PER_PACKET;
        frame_offset += FRAMES_PER_PACKET;
        first = false;

        std::this_thread::sleep_for(interval);
    }
    delete enc;
}