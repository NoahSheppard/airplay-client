#include "../include/alac_encoder.h"
#include "../vendor/alac/ALACEncoder.h"
#include "../vendor/alac/ALACAudioTypes.h"
#include <cstring>
#include <stdexcept>
#include <cassert>

static const int FRAMES_PER_PACKET = 352; 
static const int SAMPLE_RATE = 44100;
static const int CHANNELS = 2;
static const int BIT_DEPTH = 16;

int encode_alac_frame(ALACEncoder* enc, const int16_t* pcm, uint8_t* out, int out_capacity) {
    AudioFormatDescription fmt{};
    fmt.mSampleRate = SAMPLE_RATE;
    fmt.mFormatID = kALACFormatLinearPCM;
    fmt.mFormatFlags = kALACFormatFlagIsSignedInteger | kALACFormatFlagIsPacked;
    fmt.mFramesPerPacket = FRAMES_PER_PACKET;
    fmt.mChannelsPerFrame = CHANNELS;
    fmt.mBitsPerChannel = BIT_DEPTH;
    fmt.mBytesPerFrame = CHANNELS * (BIT_DEPTH / 8);
    fmt.mBytesPerPacket = fmt.mBytesPerFrame * FRAMES_PER_PACKET;

    int32_t out_bytes = out_capacity;
    AudioFormatDescription out_fmt{};
    out_fmt.mSampleRate = 44100;
    out_fmt.mFormatID = kALACFormatAppleLossless;
    out_fmt.mChannelsPerFrame = 2;
    out_fmt.mFramesPerPacket = 352;

    int err = enc->Encode(fmt, out_fmt, (uint8_t*)pcm, out, &out_bytes);
    if (err != 0) throw std::runtime_error("ALAC encode failed: " + std::to_string(err));

    return out_bytes;
}

ALACEncoder* make_alac_encoder() {
    AudioFormatDescription fmt{};
    fmt.mSampleRate = SAMPLE_RATE;
    fmt.mFormatID = kALACFormatLinearPCM;
    fmt.mFormatFlags = kALACFormatFlagIsSignedInteger | kALACFormatFlagIsPacked;
    fmt.mFramesPerPacket = FRAMES_PER_PACKET;
    fmt.mChannelsPerFrame = CHANNELS;
    fmt.mBitsPerChannel = BIT_DEPTH;
    fmt.mBytesPerFrame = CHANNELS * (BIT_DEPTH / 8);
    fmt.mBytesPerPacket = fmt.mBytesPerFrame * FRAMES_PER_PACKET;

    auto* enc = new ALACEncoder();
    enc->SetFrameSize(FRAMES_PER_PACKET);
    enc->InitializeEncoder(fmt);
    return enc;
}