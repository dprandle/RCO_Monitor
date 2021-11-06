#pragma once

#include <bits/stdint-intn.h>
#include <inttypes.h>
#include <string>

#define BITS_SET(flag, bitmask) ((flag & bitmask) == bitmask)
#define BITS_SET_ANY(flag, bitmask) ((flag & bitmask) > 0)
#define DEQUALS(left, right, eps) (((left + eps) > right) && ((left - eps) < right))

namespace util
{

std::string & to_lower(std::string & s);

bool save_data_to_file(uint8_t * data, uint32_t size, const char * fname, int mode_flags);

bool read_file_contents_to_string(const std::string & fname, std::string & contents);

std::string formatted_date(tm * time_struct);

std::string formatted_time(tm * time_struct);

std::string get_current_date_string();

std::string get_current_time_string();

uint32_t hash_id(const char * to_hash);

void delay(double ms);

bool file_exists(const std::string & name);

/// Get the count of files in the dir - ignores . and ..
uint16_t files_in_dir(const char * dir);

/// Fill the buffer with all file names from dir - ignores . and ..
/// Returns the size of the char* buffer with each element being a null terminated string
uint16_t filenames_in_dir(const char * dir, char **& buffer);

template<class T>
void zero_buf(T * buf, uint32_t size)
{
    for (uint32_t i = 0; i < size; ++i)
        buf[i] = 0;
}

template<class T>
void copy_buf(const T * src, T * dest, uint32_t size, uint32_t src_offset = 0, uint32_t dest_offset = 0)
{
    const T * src_with_offset = src + src_offset;
    T * dest_with_offset = dest + dest_offset;
    for (uint32_t i = 0; i < size; ++i)
        dest_with_offset[i] = src_with_offset[i];
}

template<class T>
uint32_t buf_len(const T * buf, uint32_t max_len = -1)
{
    int ind = 0;
    while (buf[ind] && ind != max_len)
        ++ind;
    return ind;
}
} // namespace util