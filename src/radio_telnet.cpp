#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <cmath>

#include "utility.h"
#include "main_control.h"
#include "logger.h"
#include "radio_telnet.h"
#include "timer.h"

#define STR_PRECISION(str, precision) str.substr(0, str.find('.') + precision + 1)
#define NUM_2_STR(val, prec) STR_PRECISION(std::to_string(std::round((val)*pow(10, prec)) / pow(10, prec)), prec)

namespace cmd
{
namespace str
{
const std::string ID = "ID?\n";
const std::string FREQ = "FREQ?\n";
const std::string MEAS = "MEAS?\n";
const std::string RSTAT = "RSTAT?\n";
} // namespace str
namespace ind
{
const uint8_t ID = 0;
const uint8_t FREQ = 1;
const uint8_t MEAS = 2;
const uint8_t RSTAT = 3;
} // namespace ind
} // namespace cmd

static int resp_len = 0;

TX_Params::TX_Params() : ptt_status(INVALID_VALUE), forward_power(INVALID_FLOAT), reverse_power(INVALID_FLOAT), vswr(INVALID_FLOAT)
{}

std::string TX_Params::ptt_string() const
{
    if (ptt_status == PTT_LOCAL)
        return "On (Local)";
    else if (ptt_status == PTT_REMOTE)
        return "On (Remote)";
    else if (ptt_status == PTT_TEST_RF)
        return "On (Test RF)";
    else if (ptt_status == 0)
        return "Off";
    else
        return "Invalid";
}

std::string TX_Params::to_string() const
{
    std::string ret("PTT Status: ");
    ret += ptt_string() + "\n";
    ret += "Forward Power: " + NUM_2_STR(forward_power, 2) + "W\n";
    ret += "Reflected Power: " + NUM_2_STR(reverse_power, 2) + "W\n";
    ret += "VSWR: " + NUM_2_STR(vswr, 2) + "\n";
    return ret;
}

bool TX_Params::initialized() const
{
    return ((ptt_status != INVALID_VALUE) && (forward_power > (INVALID_FLOAT + EPS)) && (reverse_power > (INVALID_FLOAT + EPS)) &&
            (vswr > (INVALID_FLOAT + EPS)));
}

RX_Params::RX_Params() : squelch_status(INVALID_VALUE), agc(INVALID_FLOAT), line_level(INVALID_FLOAT)
{}

std::string RX_Params::squelch_string() const
{
    if (squelch_status == SQUELCH_OPEN)
        return "Open";
    else if (squelch_status == SQUELCH_CLOSED)
        return "Closed";
    else
        return "Invalid";
}

std::string RX_Params::to_string() const
{
    std::string ret("Squelch Status: ");
    ret += squelch_string() + "\n";
    ret += "AGC: " + NUM_2_STR(agc, 2) + "\n";
    ret += "Line Level: " + NUM_2_STR(line_level, 2) + "dBm\n";
    return ret;
}

bool RX_Params::initialized() const
{
    return ((squelch_status != INVALID_VALUE) && (agc > (INVALID_FLOAT + EPS)) && (line_level > (INVALID_FLOAT + EPS)));
}

CM300_Radio::CM300_Radio()
    : sk(nullptr),
      freq_mhz(0.0),
      serial(),
      tx(),
      rx(),
      cur_cmd(INVALID_VALUE),
      prev_cmd(INVALID_VALUE),
      buffer_offset(0),
      response_buffer{0},
      retry_count(5),
      complete_scan_count(0)
{}

std::string CM300_Radio::radio_type() const
{
    std::string ret;
    if (serial.find('T') != std::string::npos)
        ret = TX_STR;
    else if (serial.find('R') != std::string::npos)
        ret = RX_STR;
    else
        ret = NOT_READY_STR;
    return ret;
}

std::string CM300_Radio::radio_range() const
{
    std::string ret;
    if (serial.find('U') != std::string::npos)
        ret = "UHF";
    else if (serial.find('V') != std::string::npos)
        ret = "VHF";
    else
        ret = NOT_READY_STR;
    return ret;
}

std::string CM300_Radio::to_string() const
{
    std::string ret("Serial: ");
    if (!serial.empty())
        ret += serial + "\n";
    else
        ret += "Not Initialized\n";
    ret += "Type: " + radio_type() + "\n";
    ret += "Range: " + radio_range() + "\n";
    ret += "Frequency: " + NUM_2_STR(freq_mhz, 2) + "\n";
    auto type = radio_type();
    if (type == TX_STR)
        ret += tx.to_string();
    else if (type == RX_STR)
        ret += rx.to_string();
    return ret;
}

bool CM300_Radio::initialized() const
{
    bool init = true;
    init = init && !serial.empty();
    init = init && (freq_mhz > EPS);
    return init && (rx.initialized() || tx.initialized());
}

Radio_Telnet::Radio_Telnet(uint8_t ip_lower_bound, uint8_t ip_upper_bound)
    : _ip_lb(ip_lower_bound),
      _ip_ub(ip_upper_bound),
      _logging(false),
      commands{cmd::str::ID, cmd::str::FREQ, cmd::str::MEAS, cmd::str::RSTAT},
      complete_scans(0)
{
    resp_len = strlen(RESPONSE_COMPLETE_STR);
}

Radio_Telnet::~Radio_Telnet()
{}

void Radio_Telnet::init()
{
    // Make a default logger...
    Logger_Entry def;
    def.loptions.name = "ANCE Intermod";
    
    // Enable all this crap
    def.loptions.vswr.option_a = LOPTIONA_LTHAN;
    def.loptions.agc.option_a = LOPTIONA_LTHAN;
    def.loptions.line_level.option_a = LOPTIONA_LTHAN;
    def.loptions.forward_power.option_a = LOPTIONA_LTHAN;
    
    // Actual triggers to log
    def.loptions.ptt_status.option_a = LOPTIONA_NEQUAL;
    def.loptions.ptt_status.option_c = PTT_OFF;

    def.loptions.squelch_status.option_a = LOPTIONA_EQUAL;
    def.loptions.squelch_status.option_c = SQUELCH_OPEN;
    
    def.loptions.frequency = 100;
    
    _loggers.push_back(def);

    Subsystem::init();
    int8_t size = (_ip_ub - _ip_lb + 1);
    int64_t arg = 0;

    for (int8_t i = 0; i < size; ++i)
    {
        std::string ip_last_octet = std::to_string(_ip_lb + i);
        std::string ip = "192.168.102." + ip_last_octet;

        CM300_Radio rad;
        rad.sk = new Socket();

        if (rad.sk->fd() == -1)
        {
            ilog("Failed to create socket for {} - error: ", ip, strerror(errno));
            delete rad.sk;
            continue;
        }
        ilog("Attempting to connect to radio at {} on socket fd {}", ip, rad.sk->fd());

        if (rad.sk->connect(ip, 8081, _conn_timeout) != 0)
        {
            ilog("Connection timeout to {} - no radio found", ip, strerror(errno));
            delete rad.sk;
            continue;
        }

        if (!rad.sk->start())
        {
            ilog("Could not start socket for {} on threaded fd: {}", ip, Threaded_Fd::error_string(rad.sk->error()));
            delete rad.sk;
        }
        ilog("Opened connection to radio at {} on socket fd {}", ip, rad.sk->fd());
        rad.cur_cmd = cmd::ind::FREQ;
        _radios.push_back(rad);
    }
}

void Radio_Telnet::set_max_retry_count(uint8_t max_retry_count)
{
    _max_retry_count = max_retry_count;
}

uint8_t Radio_Telnet::get_max_retry_count() const
{
    return _max_retry_count;
}

void Radio_Telnet::set_connection_timeout(const Timeout_Interval & wait_for_connection_for)
{
    _conn_timeout = wait_for_connection_for;
}

const Timeout_Interval & Radio_Telnet::get_connection_timeout() const
{
    return _conn_timeout;
}

void Radio_Telnet::release()
{
    Subsystem::release();
    for (int i = 0; i < _radios.size(); ++i)
        delete _radios[i].sk;
    _radios.clear();
}

void Logger_Entry::update_and_log_if_needed(const std::vector<CM300_Radio> & radios)
{
    ms_counter += edm.sys_timer()->dt();
    bool should_log = false;

    if (ms_counter >= loptions.frequency)
    {
        ms_counter = 0;

        for (int rind = 0; rind < radios.size(); ++rind)
        {
            CM300_Radio * prev = &prev_state[rind];
            const CM300_Radio * cur = &radios[rind];
            bool is_tx = (cur->radio_type() == TX_STR);

            // Always log on serial change or freq change
            should_log = should_log ||
                         (cur->serial != prev->serial); // || (cur->freq_mhz > (prev->freq_mhz + EPS)) || (cur->freq_mhz < (prev->freq_mhz - EPS));
            // if (should_log)
            //     ilog("Should log 1 - prev->serial {}   cur->serial {}   prev->freq {}    cur->freq {}", prev->serial,cur->serial,prev->freq_mhz,cur->freq_mhz);

            if (is_tx)
            {
                // PTT options
                should_log = should_log || ((loptions.ptt_status.option_a == LOPTIONA_DELTA) && (cur->tx.ptt_status != prev->tx.ptt_status));

                should_log =
                    should_log || ((loptions.ptt_status.option_a == LOPTIONA_NEQUAL) && (cur->tx.ptt_status != loptions.ptt_status.option_c));

                should_log = should_log || ((loptions.ptt_status.option_a == LOPTIONA_EQUAL) && (cur->tx.ptt_status == loptions.ptt_status.option_c));

                // Forward power options
                should_log = should_log || ((loptions.forward_power.option_a == LOPTIONA_DELTA) &&
                                            (std::abs(cur->tx.forward_power - prev->tx.forward_power) > loptions.forward_power.option_b));

                should_log = should_log ||
                             ((loptions.forward_power.option_a == LOPTIONA_GTHANEQUAL) && (cur->tx.forward_power >= loptions.forward_power.option_b));

                should_log =
                    should_log || ((loptions.forward_power.option_a == LOPTIONA_LTHAN) && (cur->tx.forward_power < loptions.forward_power.option_b));

                // Reverse power options
                should_log = should_log || ((loptions.reverse_power.option_a == LOPTIONA_DELTA) &&
                                            (std::abs(cur->tx.reverse_power - prev->tx.reverse_power) > loptions.reverse_power.option_b));

                should_log = should_log ||
                             ((loptions.reverse_power.option_a == LOPTIONA_GTHANEQUAL) && (cur->tx.reverse_power >= loptions.reverse_power.option_b));

                should_log =
                    should_log || ((loptions.reverse_power.option_a == LOPTIONA_LTHAN) && (cur->tx.reverse_power < loptions.reverse_power.option_b));

                // VSWR power options
                should_log =
                    should_log || ((loptions.vswr.option_a == LOPTIONA_DELTA) && (std::abs(cur->tx.vswr - prev->tx.vswr) > loptions.vswr.option_b));

                should_log = should_log || ((loptions.vswr.option_a == LOPTIONA_GTHANEQUAL) && (cur->tx.vswr >= loptions.vswr.option_b));

                should_log = should_log || ((loptions.vswr.option_a == LOPTIONA_LTHAN) && (cur->tx.vswr < loptions.vswr.option_b));
            }
            else
            {
                // RX squelch break options
                should_log =
                    should_log || ((loptions.squelch_status.option_a == LOPTIONA_DELTA) && (cur->rx.squelch_status != prev->rx.squelch_status));

                should_log = should_log ||
                             ((loptions.squelch_status.option_a == LOPTIONA_NEQUAL) && (cur->rx.squelch_status != loptions.squelch_status.option_c));

                should_log = should_log ||
                             ((loptions.squelch_status.option_a == LOPTIONA_EQUAL) && (cur->rx.squelch_status == loptions.squelch_status.option_c));

                // AGC options
                should_log =
                    should_log || ((loptions.agc.option_a == LOPTIONA_DELTA) && (std::abs(cur->rx.agc - prev->rx.agc) > loptions.agc.option_b));

                should_log = should_log || ((loptions.agc.option_a == LOPTIONA_GTHANEQUAL) && (cur->rx.agc >= loptions.agc.option_b));

                should_log = should_log || ((loptions.agc.option_a == LOPTIONA_LTHAN) && (cur->rx.agc < loptions.agc.option_b));

                // Line Level options
                should_log = should_log || ((loptions.line_level.option_a == LOPTIONA_DELTA) &&
                                            (std::abs(cur->rx.line_level - prev->rx.line_level) > loptions.line_level.option_b));

                should_log =
                    should_log || ((loptions.line_level.option_a == LOPTIONA_GTHANEQUAL) && (cur->rx.line_level >= loptions.line_level.option_b));

                should_log = should_log || ((loptions.line_level.option_a == LOPTIONA_DELTA) && (cur->rx.line_level < loptions.line_level.option_b));
            }
        }
    }
    else if (prev_state.size() != radios.size())
    {
        ms_counter = 0;
        should_log = true;
    }

    if (should_log)
    {
        prev_state = radios;
        write_radio_data_to_file();
    }
}

void Radio_Telnet::update()
{
    static std::vector<CM300_Radio *> initialized_radios;
    static bool all_radios_init = false;

    bool complete_scan = true;

    auto iter = _radios.begin();
    while (iter != _radios.end())
    {
        CM300_Radio prev = *iter;

        _update(&(*iter));
        _update_closed(&(*iter));

        if (iter->initialized())
        {}

        if (!prev.initialized() && iter->initialized())
        {
            initialized_radios.push_back(&(*iter));
            ilog("Radio at {} initialized:\n{}", iter->sk->get_ip(), iter->to_string());
        }

        complete_scan = complete_scan && (iter->complete_scan_count > complete_scans);
        ++iter;
    }

    if (complete_scan)
        ++complete_scans;

    if (all_radios_init)
    {
        for (int i = 0; i < _loggers.size(); ++i)
            _loggers[i].update_and_log_if_needed(_radios);
    }

    if (!all_radios_init && initialized_radios.size() == _radios.size())
    {
        ilog("All radios initialized");
        all_radios_init = true;

        // Setup the loggers prev state to now!
        for (int i = 0; i < _loggers.size(); ++i)
        {
            _loggers[i].prev_state = _radios;
            _loggers[i].write_headers_to_file();
        }
    }
}

std::string Logger_Entry::get_header()
{
    std::string first_row;
    std::string second_row;

    for (int i = 0; i < prev_state.size(); ++i)
    {
        std::vector<std::string> cur_row;

        CM300_Radio * rad = &prev_state[i];
        if (rad->radio_type() == TX_STR)
        {
            if (loptions.ptt_status.option_a != INVALID_VALUE)
                cur_row.push_back("PTT");
            if (loptions.forward_power.option_a != INVALID_VALUE)
                cur_row.push_back("Fwd Pwr");
            if (loptions.reverse_power.option_a != INVALID_VALUE)
                cur_row.push_back("Rev Pwr");
            if (loptions.vswr.option_a != INVALID_VALUE)
                cur_row.push_back("VSWR");
        }
        else if (rad->radio_type() == RX_STR)
        {
            if (loptions.squelch_status.option_a != INVALID_VALUE)
                cur_row.push_back("Squelch");
            if (loptions.agc.option_a != INVALID_VALUE)
                cur_row.push_back("AGC");
            if (loptions.line_level.option_a != INVALID_VALUE)
                cur_row.push_back("Line Lvl");
        }
        else
        {
            wlog("Unknown Radio Type (serial: {} freq: {})", rad->serial, rad->freq_mhz);
        }

        for (int i = 0; i < cur_row.size(); ++i)
        {
            if (i == 0)
                first_row += std::to_string(rad->freq_mhz) + " " + rad->radio_range() + " " + rad->radio_type() + " (" + rad->serial + ")";
            first_row += ",";
            second_row += cur_row[i] + ",";
        }
    }
    if (!first_row.empty())
    {
        first_row.pop_back();
        first_row = ",," + first_row;
    }
    if (!second_row.empty())
    {
        second_row.pop_back();
        second_row = "Time (h:m:s), Elapsed (s)," + second_row;
    }
    return first_row + "\n" + second_row;
}

std::string Logger_Entry::get_row()
{
    std::string row;

    for (int i = 0; i < prev_state.size(); ++i)
    {
        std::vector<std::string> cur_row;

        CM300_Radio * rad = &prev_state[i];
        if (rad->radio_type() == TX_STR)
        {
            if (loptions.ptt_status.option_a != INVALID_VALUE)
                cur_row.push_back(rad->tx.ptt_string());
            if (loptions.forward_power.option_a != INVALID_VALUE)
                cur_row.push_back(NUM_2_STR(rad->tx.forward_power, 2));
            if (loptions.reverse_power.option_a != INVALID_VALUE)
                cur_row.push_back(NUM_2_STR(rad->tx.reverse_power, 2));
            if (loptions.vswr.option_a != INVALID_VALUE)
                cur_row.push_back(NUM_2_STR(rad->tx.vswr, 2));
        }
        else if (rad->radio_type() == RX_STR)
        {
            if (loptions.squelch_status.option_a != INVALID_VALUE)
                cur_row.push_back(rad->rx.squelch_string());
            if (loptions.agc.option_a != INVALID_VALUE)
                cur_row.push_back(NUM_2_STR(rad->rx.agc, 2));
            if (loptions.line_level.option_a != INVALID_VALUE)
                cur_row.push_back(NUM_2_STR(rad->rx.line_level, 2));
        }

        for (int i = 0; i < cur_row.size(); ++i)
            row += cur_row[i] + ",";
    }
    if (!row.empty())
    {
        row.pop_back();
        row = util::get_current_time_string() + "," + NUM_2_STR(edm.sys_timer()->elapsed()/1000.0,2) + "," + row;
    }
    return row;
}

std::string Logger_Entry::get_fname()
{
    std::string fname = loptions.name + " (" + util::get_current_date_string() + ").csv";
    if (!loptions.dir_path.empty())
    {
        if (loptions.dir_path.back() != '/')
            fname = "/" + fname;
        fname = loptions.dir_path + fname;
    }
    return fname;
}

void Logger_Entry::write_headers_to_file()
{
    std::ofstream output;
    output.open(get_fname(), std::ios::out | std::ios::trunc);
    output << get_header() << "\n";
    output.close();
}

void Logger_Entry::write_radio_data_to_file()
{
    std::ofstream output;
    output.open(get_fname(), std::ios::out | std::ios::app);
    output << get_row() << "\n";
    output.close();
}

void Radio_Telnet::_update(CM300_Radio * radio)
{
    if (!radio->sk)
        return;

    uint32_t cnt = radio->sk->read(radio->response_buffer + radio->buffer_offset, BUFFER_SIZE - radio->buffer_offset);
    radio->buffer_offset += cnt; // buffer offset holds the new size

    if (radio->buffer_offset >= resp_len)
    {
        if (strncmp((char *)radio->response_buffer + (radio->buffer_offset - resp_len), RESPONSE_COMPLETE_STR, resp_len) == 0)
        {
            //ilog("Received complete packet for {} at {} (buffer size: {})!", radio->cur_cmd, radio->sk->get_ip(), radio->buffer_offset);
            _parse_response_to_radio_data(radio);
            radio->buffer_offset = 0;
            radio->prev_cmd = radio->cur_cmd;
            radio->cur_cmd = -1;
            bzero(radio->response_buffer, resp_len);
        }
    }

    if (radio->cur_cmd == INVALID_VALUE)
    {
        radio->cur_cmd = radio->prev_cmd + 1;
        if (radio->cur_cmd > cmd::ind::RSTAT)
            radio->cur_cmd = cmd::ind::ID;

        radio->sk->write(commands[radio->cur_cmd].c_str());
    }

    if (radio->cur_cmd == cmd::ind::ID && radio->prev_cmd == cmd::ind::RSTAT)
        ++radio->complete_scan_count;
}

void Radio_Telnet::_update_closed(CM300_Radio * radio)
{
    if (radio->sk && radio->sk->error().err_val != Threaded_Fd::NoError)
    {
        ilog("Closing connection to {}:{} on fd {} due to error: {}",
             radio->sk->get_ip(),
             radio->sk->get_port(),
             radio->sk->fd(),
             Threaded_Fd::error_string(radio->sk->error()));
        radio->sk->stop();
        if (radio->retry_count < _max_retry_count)
        {
            ++radio->retry_count;
            ilog("Attempting to reconnect to radio at {} on socket fd {}", radio->sk->get_ip(), radio->sk->fd());

            if (radio->sk->connect(radio->sk->get_ip(), radio->sk->get_port(), _conn_timeout) == 0)
            {
                if (radio->sk->start())
                {
                    ilog("Opened connection to radio at {} on socket fd {}", radio->sk->get_ip(), radio->sk->fd());
                }
                else
                {
                    ilog("Could not start socket for {} on threaded fd: {}", radio->sk->get_ip(), Threaded_Fd::error_string(radio->sk->error()));
                }
            }
            else
            {
                ilog("Connection timeout to {}", radio->sk->get_ip(), strerror(errno));
            }
        }
        else
        {
            ilog("Reached max retry count for {} - giving up. Bye bye to socket on fd {}.", radio->sk->get_ip(), radio->sk->fd());
            delete radio->sk;
            radio->sk = nullptr;
        }
    }
}

void Radio_Telnet::_extract_string_to_radio(CM300_Radio * radio, const std::string & str)
{
    size_t pos = str.find(':');
    if (pos == std::string::npos)
        return;
    std::string param_name = str.substr(0, pos);
    std::string param_value = str.substr(pos + 1, str.size() - pos + 1);
    if (param_name == "RADIOID")
    {
        radio->serial = param_value;
    }
    else if (param_name == "OPERATINGFREQUENCY")
    {
        radio->freq_mhz = std::stof(param_value);
    }
    else if (param_name == "FORWARDPOWER")
    {
        radio->tx.forward_power = std::stof(param_value.substr(0, param_value.size() - 1));
    }
    else if (param_name == "REFLECTEDPOWER")
    {
        radio->tx.reverse_power = std::stof(param_value.substr(0, param_value.size() - 1));
    }
    else if (param_name == "SWR")
    {
        radio->tx.vswr = std::stof(param_value);
    }
    else if (param_name == "AGC")
    {
        radio->rx.agc = std::stof(param_value.substr(0, param_value.size() - 1));
    }
    else if (param_name == "LINELEVEL")
    {
        radio->rx.line_level = std::stof(param_value.substr(0, param_value.size() - 3));
    }
    else if (param_name == "PTTSTATUS")
    {
        if (param_value == "OFF")
            radio->tx.ptt_status = PTT_OFF;
        else if (param_value == "LOCAL")
            radio->tx.ptt_status = PTT_LOCAL;
        else if (param_value == "REMOTE")
            radio->tx.ptt_status = PTT_REMOTE;
        else if (param_value == "TESTRF")
            radio->tx.ptt_status = PTT_TEST_RF;
        else
            radio->tx.ptt_status = INVALID_VALUE;
    }
    else if (param_name == "SQUELCHBREAKSTATUS")
    {
        if (param_value == "CLOSED")
            radio->rx.squelch_status = SQUELCH_CLOSED;
        else if (param_value == "OPEN")
            radio->rx.squelch_status = SQUELCH_OPEN;
        else
            radio->rx.squelch_status = INVALID_VALUE;
    }
    else
    {
        // Skip - no need to update anything
    }
}

void Radio_Telnet::_parse_response_to_radio_data(CM300_Radio * radio)
{
    std::string curline;

    for (int i = 0; i < radio->buffer_offset; ++i)
    {
        char c = radio->response_buffer[i];
        radio->response_buffer[i] = 0;
        if (c == '\n')
        {
            if (curline.find(':') != std::string::npos)
            {
                _extract_string_to_radio(radio, curline);
            }
            curline.clear();
        }
        else if (c > ' ')
        {
            curline.push_back(c);
        }
    }
}

void Radio_Telnet::enable_logging(bool enable)
{
    _logging = enable;
}

const char * Radio_Telnet::typestr()
{
    return TypeString();
}

const char * Radio_Telnet::TypeString()
{
    return "Radio_Telnet";
}
