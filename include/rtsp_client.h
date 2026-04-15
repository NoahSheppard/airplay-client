#pragma once

#include <cstdint>
#include <iostream>
#include <string>

#include <vector>
#include <map>
#include <cstring>
#include <chrono>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <thread>
#include <iomanip>
#include <algorithm>

#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

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
    uint32_t control_port;
    uint32_t timing_port;
    uint16_t server_port;
    sockaddr_in rtp_dest;
};

enum TLVType : uint8_t {
    kTLVType_Method        = 0x00,
    kTLVType_Identifier    = 0x01,
    kTLVType_Salt          = 0x02,
    kTLVType_PublicKey     = 0x03,
    kTLVType_Proof         = 0x04,
    kTLVType_EncryptedData = 0x05,
    kTLVType_State         = 0x06,
    kTLVType_Error         = 0x07,
    kTLVType_RetryDelay    = 0x08,
    kTLVType_Certificate   = 0x09,
    kTLVType_Signature     = 0x0A,
    kTLVType_Permissions   = 0x0B,
    kTLVType_FragmentData  = 0x0C,
    kTLVType_FragmentLast  = 0x0D,
    kTLVType_Separator     = 0xFF
};


// TLV8 is for homepod and first-party implementations
class TLV8 {
public:
    std::map<uint8_t, std::vector<uint8_t>> records;

    // Add a record, automatically handling data > 255 bytes
    void add(uint8_t type, const std::vector<uint8_t>& value) {
        size_t offset = 0;
        size_t total_length = value.size();
        
        if (total_length == 0) {
            records[type] = {}; // Empty value
            return;
        }

        while (offset < total_length) {
            size_t chunk_size = std::min(total_length - offset, (size_t)255);
            std::vector<uint8_t> chunk(value.begin() + offset, value.begin() + offset + chunk_size);
            
            // Append to our internal storage
            records[type].insert(records[type].end(), chunk.begin(), chunk.end());
            offset += chunk_size;
        }
    }

    // Serialize the map into a binary payload to send over HTTP
    std::vector<uint8_t> encode() const {
        std::vector<uint8_t> buffer;
        for (const auto& pair : records) {
            uint8_t type = pair.first;
            const auto& value = pair.second;
            
            size_t offset = 0;
            size_t total_length = value.size();

            if (total_length == 0) {
                buffer.push_back(type);
                buffer.push_back(0);
                continue;
            }

            while (offset < total_length) {
                uint8_t chunk_size = std::min(total_length - offset, (size_t)255);
                buffer.push_back(type);
                buffer.push_back(chunk_size);
                buffer.insert(buffer.end(), value.begin() + offset, value.begin() + offset + chunk_size);
                offset += chunk_size;
            }
        }
        return buffer;
    }
};

static const char* AIRPLAY_RSA_PUBLIC_KEY = 
"-----BEGIN RSA PUBLIC KEY-----\n"
"MIGJAoGBAOHCMkaGhKBRBGMCBpnkDaIqsJJI1JBMFMt7OIuinFmlPkRbPLCkqJmG\n"
"gYK3MBOGObVTpLFRqkQJN46YXqCNBd0y8m3vMEPFdPIWMYNpqBr8TfBayKFqJPIU\n"
"lBaHaBAHXOTRCVoQWIVBMiTsEjxMRZJPEcJwFIYRhBz+ht9VAgMBAAE=\n"
"-----END RSA PUBLIC KEY-----\0";

// Init
void rtsp_init(AirPlaySession& session, const std::string& host, int port);

// AirPlay 1 Steps
bool rtsp_options(AirPlaySession& session, bool DEBUG);
bool rtsp_announce(AirPlaySession& session, bool DEBUG);
bool rtsp_setup(AirPlaySession& session, bool DEBUG);
bool rtsp_record(AirPlaySession& session, bool DEBUG);

// AirPlay 2 Steps
// bool rtsp_pair_setup(AirPlaySession& session, bool DEBUG); //  This will come into play later when we support first party recievers

// Utils
std::string base64_encode(const uint8_t* data, size_t len);