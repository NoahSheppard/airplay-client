#pragma once
#include <cstdint>
class ALACEncoder;

ALACEncoder* make_alac_encoder();
int encode_alac_frame(ALACEncoder* enc, const int16_t* pcm, uint8_t* out, int out_capacity);