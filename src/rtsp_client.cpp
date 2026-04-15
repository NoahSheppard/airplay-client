#include "../include/rtsp_client.h"
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

#include <sstream>
#include <sys/socket.h>
#include <unistd.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>

bool rtsp_options(AirPlaySession& session) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    session.control_socket = sock;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(session.port);
    if (inet_pton(AF_INET, session.host.c_str(), &addr.sin_addr) <= 0) {
        std::cout << "[rtsp_client.cpp] Closing socket (1)" << std::endl;
        close(session.control_socket);
        return false;
    }

    if (connect(session.control_socket, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cout << "[rtsp_client.cpp] Closing socket (2)" << std::endl;
        close(session.control_socket);
        return false;
    }

    std::ostringstream req;
    req << "OPTIONS rtsp://" << session.host << ":" << session.port << "/ RTSP/1.0\r\n"
        << "CSeq: " << session.cseq << "\r\n"
        << "User-Agent: noahsh-rtsp-client\r\n"
        << "\r\n";

    std::string request = req.str();
    if (send(session.control_socket, request.c_str(), request.size(), 0) < 0) {
        std::cout << "[rtsp_client.cpp] Closing socket (3)" << std::endl;
        close(session.control_socket);
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

    std::cout << "[rtsp_client.cpp] Response=" << response << std::endl;

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

    std::cout << "[rtsp_client.cpp] (FINAL) cseq_response=" << cseq_response << std::endl;
    std::cout << "[rtsp_client.cpp] (FINAL) expected=" << session.cseq << std::endl;

    
    bool ok = (cseq_response == (int)session.cseq);
    session.cseq += 1;
    return ok;
    
}