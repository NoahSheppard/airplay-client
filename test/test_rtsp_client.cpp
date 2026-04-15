#include <iostream>
#include "../include/rtsp_client.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << "Usage: ./test_rstp_client ip port." << std::endl;
        return 0;
    }
    AirPlaySession session;
    session.host = argv[1];
    session.port = std::stoi(argv[2]);
    session.cseq = 1;
    bool rtsp_options_ok = rtsp_options(session);
    std::cout << rtsp_options_ok << std::endl;
    return 0;
}