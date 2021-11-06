#pragma once

#include <vector>
#include <unordered_map>

#include "subsystem.h"
#include "socket.h"

#define MAP_CONTAINS(map,param) map.find(param) != map.end()

class Socket;

const int8_t COMMAND_COUNT = 4;
const int16_t BUFFER_SIZE = 512;
const char RESPONSE_COMPLETE_STR[] = "CM300V2> ";

const std::string RX_STR = "RX";
const std::string TX_STR = "TX";
const std::string NOT_READY_STR = "Not Initialized";

namespace cmd
{
namespace str
{
extern const std::string ID;
extern const std::string FREQ;
extern const std::string MEAS;
extern const std::string RSTAT;
} // namespace str
namespace ind
{
extern const uint8_t ID;
extern const uint8_t FREQ;
extern const uint8_t MEAS;
extern const uint8_t RSTAT;
} // namespace ind
} // namespace cmd

const uint8_t INVALID_VALUE = -1;
const float INVALID_FLOAT = -100.0f;
const float EPS = 0.001f;

const uint8_t PTT_OFF = 1;
const uint8_t PTT_LOCAL = 2;
const uint8_t PTT_REMOTE = 4;
const uint8_t PTT_TEST_RF = 8;

const uint8_t LOPTIONA_DELTA = 1;
const uint8_t LOPTIONA_NEQUAL = 2;
const uint8_t LOPTIONA_EQUAL = 4;
const uint8_t LOPTIONA_GTHANEQUAL = 2;
const uint8_t LOPTIONA_LTHAN = 4;

const uint8_t SQUELCH_CLOSED = 1;
const uint8_t SQUELCH_OPEN = 2;

struct Command_Info
{
    std::string name;
    std::string resp_key;
};

std::string ptt_string(uint8_t status);
std::string squelch_string(uint8_t status);

struct TX_Params
{
    TX_Params();
    std::string ptt_string() const;
    std::string to_string() const;
    bool initialized() const;

    uint8_t ptt_status;
    float forward_power;
    float reverse_power;
    float vswr;
};

struct RX_Params
{
    RX_Params();
    std::string squelch_string() const;
    std::string to_string() const;
    bool initialized() const;

    uint8_t squelch_status;
    float agc;
    float line_level;
};

struct CM300_Radio
{
    CM300_Radio();
    std::string radio_type() const;
    std::string radio_range() const;
    std::string to_string() const;
    bool initialized() const;

    Socket * sk;
    float freq_mhz;
    std::string serial;
    TX_Params tx;
    RX_Params rx;

    uint8_t cur_cmd;
    uint8_t prev_cmd;
    uint16_t buffer_offset;
    uint8_t response_buffer[BUFFER_SIZE];
    uint8_t retry_count;
    size_t complete_scan_count;
};

template<class T>
struct Log_Item_Option
{
    Log_Item_Option() : val(), enabled(false)
    {}
    T val;
    bool enabled;
};

struct Log_Option_Group
{
    Log_Option_Group() {}
    Log_Item_Option<std::string> title;
    Log_Item_Option<float> greater_than;
    Log_Item_Option<float> less_than;
    Log_Item_Option<float> change;
    Log_Item_Option<float> percent_change;
    Log_Item_Option<int32_t> equal;
};

struct Logger_Options
{
    Logger_Options(): dir_path(), frequency(0), log_changes_to_status(false) {}
    std::string dir_path;
    uint32_t frequency;
    bool log_changes_to_status;

    /* 
    The option keys for log item options so far are listed below:
        ptt_status
        squelch_status
        forward_power
        reverse_power
        vswr
        agc
        line_level
     */
    std::unordered_map<std::string, Log_Option_Group> item_options;
};

struct Logger_Entry
{
    Logger_Entry() : ms_counter(0)
    {}
    void update_and_log_if_needed(const std::vector<CM300_Radio> & radios);
    void write_headers_to_file();
    void write_radio_data_to_file();
    std::string get_header();
    std::string get_row();
    std::string get_fname();

    Logger_Options loptions;
    double ms_counter;
    std::string name;
    std::vector<CM300_Radio> prev_state;
};

class Radio_Telnet : public Subsystem
{
  public:
    Radio_Telnet();

    ~Radio_Telnet();

    void init(Config_File * config);

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
    void _extract_string_to_radio(CM300_Radio * radio, const std::string & str);
    void _set_options_from_config_file(Config_File * cfg);

    bool _logging;
    int8_t _ip_lb;
    int8_t _ip_ub;
    uint8_t _max_retry_count;
    Timeout_Interval _conn_timeout;

    size_t _cur_cmd;
    std::string commands[COMMAND_COUNT];
    std::unordered_map<std::string, Logger_Entry> _loggers;

    std::vector<CM300_Radio> _radios;
    size_t complete_scans;
};