#pragma once

#include <vector>
#include "subsystem.h"
#include "socket.h"
class Socket;

const int8_t COMMAND_COUNT = 4;
const int16_t BUFFER_SIZE = 512;
const char RESPONSE_COMPLETE_STR[] = "CM300V2> ";

namespace cmd
{
namespace str
{
extern const std::string FREQ;
extern const std::string ID;
extern const std::string MEAS;
extern const std::string RSTAT;
} // namespace str
namespace ind
{
extern const uint8_t FREQ;
extern const uint8_t ID;
extern const uint8_t MEAS;
extern const uint8_t RSTAT;
} // namespace ind
} // namespace cmd

const uint8_t INVALID_VALUE = -1;

struct Command_Info
{
    std::string name;
    std::string resp_key;
};

struct TX_Params
{
    TX_Params() : ptt_status(INVALID_VALUE), forward_power(0.0f), reflected_power(0.0f), vswr(0.0f)
    {}
    uint8_t ptt_status;
    float forward_power;
    float reflected_power;
    float vswr;
};

struct RX_Params
{
    RX_Params() : squelch_status(INVALID_VALUE), agc(0.0)
    {}
    uint8_t squelch_status;
    float agc;
};

struct CM300_Radio
{
    CM300_Radio()
        : sk(nullptr),
          type(INVALID_VALUE),
          uorv(INVALID_VALUE),
          freq_mhz(0.0),
          serial(),
          tx(),
          rx(),
          cur_cmd(INVALID_VALUE),
          prev_cmd(INVALID_VALUE),
          buffer_offset(0),
          response_buffer{0},
          retry_count(5)
    {}
    Socket * sk;
    uint8_t type;
    uint8_t uorv;
    float freq_mhz;
    std::string serial;
    TX_Params tx;
    RX_Params rx;

    uint8_t cur_cmd;
    uint8_t prev_cmd;
    uint16_t buffer_offset;
    uint8_t response_buffer[BUFFER_SIZE];
    uint8_t retry_count;
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

    void set_max_retry_count(uint8_t max_retry_count);

    uint8_t get_max_retry_count() const;

    void set_connection_timeout(const Timeout_Interval & wait_for_connection_for);

    const Timeout_Interval & get_connection_timeout() const;

    const char * typestr();

    static const char * TypeString();

  private:
    void _update_closed(CM300_Radio * radio);
    void _update(CM300_Radio * radio);
    void _parse_response_to_radio_data(CM300_Radio * radio);

    bool _logging;
    int8_t _ip_lb;
    int8_t _ip_ub;
    uint8_t _max_retry_count;
    Timeout_Interval _conn_timeout;

    size_t _cur_cmd;
    std::string commands[COMMAND_COUNT];

    std::vector<CM300_Radio> _radios;
};