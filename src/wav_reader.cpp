#include "../include/wav_reader.h"
#include <fstream>
#include <stdexcept>
#include <cstring>

WavFile load_wav(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open: " + path);

    char riff[4]; f.read(riff, 4);
    if (memcmp(riff, "RIFF", 4) != 0)
        throw std::runtime_error("Not a RIFF file");

    f.seekg(22); uint16_t channels; f.read((char*)&channels, 2);
    f.seekg(24); uint16_t sample_rate; f.read((char*)&sample_rate, 4);
    f.seekg(34); uint16_t bit_depth; f.read((char*)&bit_depth, 2);

    if (channels != 2 || sample_rate != 44100 || bit_depth != 16)
        throw std::runtime_error("WAV must be 44100 Hz, 16-bit, stereo");

    f.seekg(36);
    char chunk_id[4];
    uint32_t chunk_size;
    while (f.read(chunk_id, 4) && f.read((char*)&chunk_size, 4)) {
        if (memcmp(chunk_id, "data", 4) == 0) break;
        f.seekg(chunk_size, std::ios::cur);
    }

    WavFile wav;
    wav.total_frames = chunk_size / (channels * (bit_depth / 8));
    wav.samples.resize(wav.total_frames * channels);
    f.read((char*)wav.samples.data(), chunk_size);
    return wav;
}