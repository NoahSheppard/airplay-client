cmake_minimum_required(VERSION 3.20)
project(airplay_sender C CXX)

set(CMAKE_CXX_STANDARD 17)

include_directories(include vendor)

add_library(airplay_core
    src/discovery.cpp
    src/rtsp_client.cpp
    src/rtp_sender.cpp
    src/alac_encoder.cpp
    src/aes_crypto.cpp
    src/wav_reader.cpp
    vendor/alac/ALACEncoder.cpp
    vendor/alac/ALACDecoder.cpp
    vendor/alac/ag_dec.c
    vendor/alac/ag_enc.c
    vendor/alac/ALACBitUtilities.c
    vendor/alac/dp_dec.c
    vendor/alac/dp_enc.c
    vendor/alac/EndianPortable.c
    vendor/alac/matrix_dec.c
    vendor/alac/matrix_enc.c
)

add_executable(airplay_sender src/main.cpp)
target_link_libraries(airplay_sender airplay_core ssl crypto)

add_executable(test_discovery test/test_discovery.cpp)
target_link_libraries(test_discovery airplay_core)

add_executable(test_rtsp_client test/test_rtsp_client.cpp)
target_link_libraries(test_rtsp_client airplay_core ssl crypto)