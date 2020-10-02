#include <cstring>
#include <stdlib.h>
#include <stdio.h>

#include "utility.h"
#include "shared_structs.h"

void parse_filename(const char * fname, Firmware_Header & to_fill)
{
    char copy_buf[40] = {};
    strcpy(copy_buf,fname);
    char * str = strtok(copy_buf,"v");
    int i = 4;
    while (str != nullptr)
    {
        str = strtok(nullptr,".");
        if (i < FIRMWARE_HEADER_SIZE)
        {
            to_fill.data[i] = (uint8_t)atoi(str);
            ++i;
        }
    }
}

char * parse_firmware_header(const Firmware_Header & fmh)
{
    static char fname[50];
    memset(fname,0,50);
    sprintf(fname, "/home/ubuntu/bin/RCO_Monitor-v%d.%d.%d.rpi",fmh.v_major,fmh.v_minor,fmh.v_patch);
    return fname;
}
