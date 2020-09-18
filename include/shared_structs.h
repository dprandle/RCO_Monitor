#pragma once
#include <cstdint>

const int FIRMWARE_HEADER_SIZE = 7;
struct Firmware_Header
{
    Firmware_Header(): byte_size(0), v_major(0), v_minor(0), v_patch(0) {}
    union
    {
        uint8_t data[FIRMWARE_HEADER_SIZE];
        struct
        {
            uint32_t byte_size;
            uint8_t v_major;
            uint8_t v_minor;
            uint8_t v_patch;
        };
    };
};
