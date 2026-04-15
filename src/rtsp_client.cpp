#include "../include/rtsp_client.h"
#include "../include/other.h"

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

void rtsp_init(AirPlaySession& session, const std::string& host, int port) {
    session.host = host;
    session.port = port;
    session.cseq = 1;
    session.control_socket = -1;
    session.audio_socket = -1;
    session.server_port = 0;

    uint8_t raw[4];
    RAND_bytes(raw, sizeof(raw));
    char hex[9];
    snprintf(hex, sizeof(hex), "%02x%02x%02x%02x", raw[0], raw[1], raw[2], raw[3]);
    session.session_id = hex;
}

bool rtsp_options(AirPlaySession& session, bool DEBUG) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    session.control_socket = sock;

    sockaddr_in addr{}; // init the socket
    addr.sin_family = AF_INET;
    addr.sin_port = htons(session.port);
    if (inet_pton(AF_INET, session.host.c_str(), &addr.sin_addr) <= 0) {
        if (DEBUG) std::cout << RED_BOLD << "[rtsp_client.cpp] Closing socket (1)" << WHITE << std::endl;
        close(session.control_socket); // if couldn't create the socket
        return false;
    }

    if (connect(session.control_socket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        if (DEBUG) std::cout << RED_BOLD << "[rtsp_client.cpp] Closing socket (2)" << WHITE << std::endl;
        close(session.control_socket); // if cannot connect to host
        return false;
    }

    uint8_t challenge_bytes[16];
    RAND_bytes(challenge_bytes, sizeof(challenge_bytes));
    std::string apple_challenge = base64_encode(challenge_bytes, sizeof(challenge_bytes));
    apple_challenge.erase(std::remove(apple_challenge.begin(), apple_challenge.end(), '='), apple_challenge.end());

    std::ostringstream req;
    req << "OPTIONS rtsp://" << session.host << ":" << session.port << "/ RTSP/1.0\r\n"
        << "CSeq: " << session.cseq << "\r\n"
        << "User-Agent: iTunes/10.6 (Macintosh; Intel Mac OS X 10.7.3) AppleWebKit/535.18.5\r\n"
        << "Client-Instance: 56B29BB6CB904862\r\n"
        << "Apple-Challenge: " << apple_challenge << "\r\n"
        << "\r\n";

    std::string request = req.str();
    if (send(session.control_socket, request.c_str(), request.size(), 0) < 0) {
        if (DEBUG) std::cout << RED_BOLD << "[rtsp_client.cpp] Closing socket (3)" << WHITE << std::endl;
        close(session.control_socket); // if data doesn't send
        return false;
    }

    sockaddr_in local_addr {};
    socklen_t len = sizeof(local_addr);
    getsockname(session.control_socket, (sockaddr*)&local_addr, &len);
    char local_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));

    char buffer[4096];
    std::string response;
    ssize_t n; 

    while ((n = recv(session.control_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = 0;
        response += buffer;
        if (response.find("\r\n\r\n") != std::string::npos) break;
    }

    if (DEBUG) std::cout << YELLOW << "[rtsp_client.cpp] Response=" << response << WHITE << std::endl;

    if (response.empty()) return false;

    std::istringstream resp_stream(response);
    std::string rtsp_version;
    int status_code = 0;

    resp_stream >> rtsp_version >> status_code;

    if (status_code != 200) return false;

    std::istringstream lines(response);
    std::string line;
    int cseq_response = -1;

    while (std::getline(lines, line)) {
        if (line.rfind("CSeq:", 0) == 0) {
            std::string val = line.substr(5);
            cseq_response = std::stoi(val);
            break;
        }
    }

    if (DEBUG) std::cout << CYAN << "[rtsp_client.cpp] (FINAL) cseq_response=" << cseq_response << WHITE << std::endl;
    if (DEBUG) std::cout << CYAN << "[rtsp_client.cpp] (FINAL) expected=" << session.cseq << WHITE << std::endl;
    
    bool ok = (cseq_response == (int)session.cseq);
    session.cseq += 1;
    return ok;
}

bool rtsp_announce(AirPlaySession& session, bool DEBUG) {
    RAND_bytes(session.aes_key, 16);
    RAND_bytes(session.aes_iv, 16);

    EVP_PKEY* pkey = nullptr;
    BIO* bio = BIO_new_mem_buf(AIRPLAY_RSA_PUBLIC_KEY, -1);
    PEM_read_bio_PUBKEY(bio, &pkey, nullptr, nullptr);
    BIO_free(bio);

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pkey, nullptr);
    EVP_PKEY_encrypt_init(ctx);
    EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING);

    size_t encrypted_len = 16;
    uint8_t encrypted_key[16];
    EVP_PKEY_encrypt(ctx, encrypted_key, &encrypted_len, session.aes_key, 16);
    

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pkey);

    std::string base64_encrypted_key = base64_encode(encrypted_key, sizeof(encrypted_key));
    base64_encrypted_key.erase(std::remove(base64_encrypted_key.begin(), base64_encrypted_key.end(), '='), base64_encrypted_key.end());

    std::string base64_iv = base64_encode(session.aes_iv, 16);
    base64_iv.erase(std::remove(base64_iv.begin(), base64_iv.end(), '='), base64_iv.end());

    // Apple-Challenge because of course there is
    uint8_t challenge_bytes[16];
    RAND_bytes(challenge_bytes, sizeof(challenge_bytes));
    std::string apple_challenge = base64_encode(challenge_bytes, sizeof(challenge_bytes));
    apple_challenge.erase(std::remove(apple_challenge.begin(), apple_challenge.end(), '='), apple_challenge.end());

    char local_ip[INET_ADDRSTRLEN];
    sockaddr_in local_addr {};
    socklen_t len = sizeof(local_addr);
    getsockname(session.control_socket, (sockaddr*)&local_addr, &len);
    inet_ntop(AF_INET, &local_addr.sin_addr, local_ip, sizeof(local_ip));

    std::ostringstream sdp;
    sdp << "v=0\r\n"
        << "o=iTunes 1234 0 IN IP4 " << session.host << "\r\n"
        << "s=iTunes\r\n"
        << "c=IN IP4 " << session.host << "\r\n"
        << "t=0 0\r\n"
        << "m=audio 0 RTP/AVP 96\r\n"
        << "a=rtpmap:96 AppleLossless\r\n"
        << "a=fmtp:96 352 0 16 40 10 14 2 255 0 0 44100\r\n"
        << "a=rsaaeskey:" << base64_encrypted_key << "\r\n"
        << "a=aesiv:" << base64_iv << "\r\n";

    std::cout << CYAN_BOLD << "[rtsp_client.cpp] B64 RSA Key='" << base64_encrypted_key << "'\n" << WHITE << std::endl;
    std::cout << CYAN_BOLD << "[rtsp_client.cpp] B64 IV='" << base64_iv << "'\n" << WHITE << std::endl;

    std::string sdp_body = sdp.str();

    // now build headers to get content length
    std::ostringstream req;
    req << "ANNOUNCE rtsp://" << session.host << ":" << session.port << "/" << session.session_id << " RTSP/1.0\r\n"
        << "CSeq: " << session.cseq << "\r\n"
        << "User-Agent: iTunes/10.6 (Macintosh; Intel Mac OS X 10.7.3) AppleWebKit/535.18.5\r\n"
        << "Client-Instance: 56B29BB6CB904862\r\n"
        << "Apple-Challenge: " << apple_challenge << "\r\n"
        << "Content-Type: application/sdp\r\n"
        << "Content-Length: " << sdp_body.size() << "\r\n"
        << "\r\n"
        << sdp_body;

    std::string request = req.str();
    if (send(session.control_socket, request.c_str(), request.size(), 0) < 0) {
        if (DEBUG) std::cout << RED_BOLD << "[rtsp_client.cpp] Closing socket (3)" << WHITE << std::endl;
        close(session.control_socket); // if data doesn't send
        return false;
    }

    char buffer[4096];
    std::string response;
    ssize_t n; 

    while ((n = recv(session.control_socket, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[n] = 0;
        response += buffer;
        if (response.find("\r\n\r\n") != std::string::npos) break;
    }

    if (DEBUG) std::cout << YELLOW << "[rtsp_client.cpp] Response=" << response << WHITE << std::endl;

    if (response.empty()) return false;

    std::istringstream resp_stream(response);
    std::string rtsp_version;
    int status_code = 0;

    resp_stream >> rtsp_version >> status_code;

    if (status_code != 200) return false;

    std::istringstream lines(response);
    std::string line;
    int cseq_response = -1;

    while (std::getline(lines, line)) {
        if (line.rfind("CSeq:", 0) == 0) {
            std::string val = line.substr(5);
            cseq_response = std::stoi(val);
            break;
        }
    }

    bool ok = (cseq_response == (int)session.cseq);
    session.cseq += 1;
    return ok;
}

bool rtsp_setup(AirPlaySession& session, bool DEBUG) {
    session.audio_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (session.audio_socket < 0) return false;

    struct sockaddr_in local{};
    local.sin_family = AF_INET;
    local.sin_addr.s_addr = INADDR_ANY;
    local.sin_port = htons(6001); // client-decided port
    bind(session.audio_socket, (struct sockaddr*)&local, sizeof(local));

    std::ostringstream req;
    req << "SETUP rtsp://" << session.host << "/" << session.session_id << " RTSP/1.0\r\n"
        << "CSeq: " << session.cseq << "\r\n"
        << "Transport: RTP/AVP/UDP;unicast;interleaved=0-1;mode=record;"
        << "control_port=6001;timing_port=6002\r\n"
        << "User-Agent: iTunes/12.13.1\r\n"
        << "\r\n";

    std::string request = req.str();
    send(session.control_socket, request.c_str(), request.size(), 0);

    char buf[4096] = {};
    int n = recv(session.control_socket, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    std::string response(buf, n);

    if (response.find("RTSP/1.0 200") == std::string::npos) return false;

    std::cout << YELLOW << "[rtsp_client.cpp] Response=" << response << WHITE << std::endl;

    auto si = response.find("Session: ");
    if (si != std::string::npos) {
        si += 9;
        auto ei = response.find("\r\n", si);
        session.session_token = response.substr(si, ei - si);
        auto sc = session.session_token.find(';');
        if (sc != std::string::npos) session.session_token = session.session_token.substr(0, sc);
    } else {
        std::cout << RED << "[rtsp_client.cpp] No session id." << WHITE << std::endl;
    }

    auto cp = response.find("control_port=");
    try {
        if (cp!= std::string::npos) {
            cp += 12;
            auto ep = response.find_first_not_of("0123456789", cp);
            session.control_port = (uint16_t)std::stoi(response.substr(cp, ep - cp));
        }
    } catch (std::exception& e) {
        std::cout << RED << "[rtsp_client.cpp] Could not find Control Port" << WHITE << std::endl;
    }

    auto tp = response.find("timing_port=");
    try {
        if (tp!= std::string::npos) {
            tp += 12;
            auto ep = response.find_first_not_of("0123456789", tp);
            session.timing_port = (uint16_t)std::stoi(response.substr(tp, ep - tp));
        }
    } catch (std::exception& e) {
        std::cout << RED << "[rtsp_client.cpp] Could not find Timing Port" << WHITE << std::endl;
    }

    auto sp = response.find("server_port=");
    try {
        if (sp!= std::string::npos) {
            sp += 12;
            auto ep = response.find_first_not_of("0123456789", sp);
            session.server_port = (uint16_t)std::stoi(response.substr(sp, ep - sp));
        }
    } catch (std::exception& e) {
        std::cout << RED << "[rtsp_client.cpp] Could not find Server Port" << WHITE << std::endl;
    }

    if (session.session_token.empty() || session.server_port == 0) return false;

    memset(&session.rtp_dest, 0, sizeof(session.rtp_dest));
    session.rtp_dest.sin_family = AF_INET;
    session.rtp_dest.sin_port = htons(session.server_port);
    inet_pton(AF_INET, session.host.c_str(), &session.rtp_dest.sin_addr);

    session.cseq++;
    return true; 
}

bool rtsp_record(AirPlaySession& session, bool DEBUG) {
    std::ostringstream req;
    req << "RECORD rtsp://" << session.host << "/" << session.session_id << " RTSP/1.0\r\n"
        << "CSeq: " << session.cseq << "\r\n"
        << "Session: " << session.session_token << "\r\n"
        << "Range: npt=0-\r\n"
        << "RTP-Info: seq=" << session.sequence_number
        << ";rtptime=" << session.timestamp << "\r\n"
        << "User-Agent: iTunes/12.13.1\r\n"
        << "\r\n";

    std::string request = req.str();
    send(session.control_socket, request.c_str(), request.size(), 0);

    char buf[4096] = {};
    int n = recv(session.control_socket, buf, sizeof(buf) - 1, 0);
    if (n <= 0) return false;
    std::string response(buf, n);

    std::cout << YELLOW << "[rtsp_client.cpp] Response=" << response << WHITE << std::endl;

    return response.find("RTSP/1.0 200") != std::string::npos;
}

std::string base64_encode(const uint8_t* data, size_t len) {
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    BIO_push(b64, mem);
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_write(b64, data, len);
    BIO_flush(b64);

    char* out;
    size_t out_len = BIO_get_mem_data(mem, &out);
    std::string result(out, out_len);

    BIO_free_all(b64);
    return result;
}