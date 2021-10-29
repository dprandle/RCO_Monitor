#pragma once

#include <vector>
#include "subsystem.h"
class Socket;

const int8_t COMMAND_COUNT = 5;

extern const std::string CMD_FREQ;
extern const std::string CMD_ID;
extern const std::string CMD_MEAS;
extern const std::string CMD_TYPE;
extern const std::string CMD_RSTAT;



struct TX_Params
{
    int8_t ptt_status;
    float forward_power;
    float reflectd_power;
    float vswr;
};

struct RX_Params
{
    int8_t squelch_status;
    float agc;
};

struct CM300_Radio
{
    Socket * sk;
    int8_t type;
    int8_t uorv;
    float freq_mhz;
    std::string serial;
    TX_Params tx;
    RX_Params rx;
};

class Radio_Telnet : public Subsystem
{
  public:
    Radio_Telnet(uint8_t ip_lower_bound, uint8_t ip_upper_bound);

    ~Radio_Telnet();

    void init();

    void release();

    void update();

    void enable_logging(bool enable);

    const char * typestr();

    static const char * TypeString();

  private:
    void _update_closed();
    
    bool _logging;
    int8_t _ip_lb;
    int8_t _ip_ub;

    size_t _cur_cmd;
    std::string commands[COMMAND_COUNT];

    std::vector<CM300_Radio> _radios;
};