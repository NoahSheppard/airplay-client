#include <iostream>
#include "../include/rtsp_client.h"
#include "../include/rtp_sender.h"
#include "../include/other.h"
#include "../include/wav_reader.h"

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cout << RED_BOLD << "Usage: ./test_rstp_client ip port (optional: /path/to/wav)" << WHITE << std::endl;
        return 0;
    }
    AirPlaySession session;
    rtsp_init(session, argv[1], std::stoi(((std::string)argv[2])));

    std::cout << BLUE << "[test_rtsp_client.cpp] Commencing RTSP Options request... " << WHITE << std::endl;
    bool rtsp_options_ok = rtsp_options(session, true);
    std::cout << (rtsp_options_ok ? GREEN_BOLD : RED) << "[test_rtsp_client.cpp] RTSP Options: " << rtsp_options_ok << WHITE << std::endl;
    if (!rtsp_options_ok) return 1;

    std::cout << BLUE << "\n[test_rtsp_client.cpp] Commencing RTSP Announce request... " << WHITE << std::endl;
    bool rtsp_announce_ok = rtsp_announce(session, true);
    std::cout << (rtsp_announce_ok ? GREEN_BOLD : RED) << "[test_rtsp_client.cpp] RTSP Announce: " << rtsp_announce_ok << WHITE << std::endl;
    if (!rtsp_announce_ok) return 1;

    std::cout << BLUE << "\n[test_rtsp_client.cpp] Commencing RTSP Setup request... " << WHITE << std::endl;
    bool rtsp_setup_ok = rtsp_setup(session, true);
    std::cout << (rtsp_setup_ok ? GREEN_BOLD : RED) << "[test_rtsp_client.cpp] RTSP Setup: " << rtsp_setup_ok << WHITE << std::endl;
    if (!rtsp_setup_ok) return 1;

    std::cout << "server_port=" << session.server_port << ", session_token=" << session.session_token.c_str() << std::endl;

    std::cout << BLUE << "\n[test_rtsp_client.cpp] Commencing RTSP Record request... " << WHITE << std::endl;
    bool rtsp_record_ok = rtsp_record(session, true);
    std::cout << (rtsp_record_ok ? GREEN_BOLD : RED) << "[test_rtsp_client.cpp] RTSP Record: " << rtsp_record_ok << WHITE << std::endl;
    if (!rtsp_record_ok) return 1;

    std::cout << GREEN_BOLD << "[test_rtsp_client.cpp] Final Data: " << WHITE << std::endl;

    std::cout << "  control_socket='" << session.control_socket << "'," <<std::endl;
    std::cout << "  audio_socket='" << session.audio_socket << "'," << std::endl;
    std::cout << "  host='" << session.host << "'," << std::endl;
    std::cout << "  port='" << session.port << "'," << std::endl;
    std::cout << "  session_token='" << session.session_token << "'," << std::endl;
    std::cout << "  session_id='" << session.session_id << "'," << std::endl;
    std::cout << "  cseq='" << session.cseq << "'," << std::endl;
    std::cout << "  aes_key='" << session.aes_key << "'," << std::endl;
    std::cout << "  aes_iv='" << session.aes_iv << "'," << std::endl;
    std::cout << "  sequence_number='" << session.sequence_number << "'," << std::endl;
    std::cout << "  timestamp='" << session.timestamp << "'," << std::endl;
    std::cout << "  control_port='" << session.control_port << "'," << std::endl;
    std::cout << "  timing_port='" << session.timing_port << "'," << std::endl;
    std::cout << "  server_port='" << session.server_port << "'," << std::endl;
    std::cout << "  rtp_dest='" << session.rtp_dest.sin_addr.s_addr << ":" << session.rtp_dest.sin_port << "'" << std::endl;

    if (argc < 4) {
        std::cout << YELLOW_BOLD << "[test_rtsp_client.cpp] No WAV file given, sending 5 seconds of silence" << WHITE << std::endl;
        send_silence(session, 5, true);
        std::cout << GREEN_BOLD << "[test_rtsp_client.cpp] Done." << std::endl;
    } else {
        WavFile wav = load_wav(argv[3]);
        std::cout << BLUE_BOLD << "[test_rtsp_client.cpp] Loaded " << argv[3] << " -- " << wav.total_frames << " frames (" << wav.total_frames / 44100 << " sec)" << WHITE << std::endl;
        send_audio(session, wav.samples.data(), wav.total_frames);
    }

    return 0;
}