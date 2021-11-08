#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <cmath>
#include <set>
#include <random>

#include "config_file.h"
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

void default_radio_params(CM300_Radio * rad)
{
    rad->tx.forward_power = 0;
    rad->tx.reverse_power = 0;
    rad->tx.vswr = -1.0;
    rad->tx.ptt_status = PTT_OFF;
    rad->rx.agc = 1.61;
    rad->rx.line_level = -46.0;
    rad->rx.squelch_status = SQUELCH_CLOSED;
}

void create_simulated_radio_set(std::vector<CM300_Radio> & radios, int vcount, int ucount)
{
    std::string uprefix("2U");
    std::set<int> used_rnums;

    srand((unsigned)time(0));

    double f = 0;
    for (int i = 0; i < vcount * 2; ++i)
    {
        CM300_Radio rad;
        int rnum = 0;
        rad.serial = "2V";

        if (i % 2)
            rad.serial.push_back('T');
        else
        {
            rad.serial.push_back('R');
            rnum = (rand() % 1400) + 11800;
            f = double(rnum) / 100.0;
        }

        do
        {
            rnum = (rand() % 999999) + 1;
        } while (!used_rnums.insert(rnum).second);
        std::string numstr = std::to_string(rnum);
        numstr.insert(0, 6 - numstr.size(), '0');
        rad.serial.append(numstr);

        rad.freq_mhz = f;
        default_radio_params(&rad);
        radios.push_back(rad);
    }
    for (int i = 0; i < ucount * 2; ++i)
    {
        CM300_Radio rad;
        rad.serial = "2U";
        int rnum = 0;

        if (i % 2)
            rad.serial.push_back('T');
        else
        {
            rad.serial.push_back('R');
            rnum = (rand() % 1400) + 11800;
            f = double(rnum) / 100.0;
        }

        do
        {
            rnum = (rand() % 999999) + 1;
        } while (!used_rnums.insert(rnum).second);
        std::string numstr = std::to_string(rnum);
        numstr.insert(0, 6 - numstr.size(), '0');
        rad.serial.append(numstr);

        rad.freq_mhz = f;
        default_radio_params(&rad);
        radios.push_back(rad);
    }
}

std::string ptt_string(uint8_t status)
{
    if (status == PTT_OFF)
        return "Off";
    else if (status == PTT_LOCAL)
        return "Key(L)";
    else if (status == PTT_REMOTE)
        return "Key(R)";
    else if (status == PTT_TEST_RF)
        return "Key(T)";
    else
        return "Invalid";
}

std::string squelch_string(uint8_t status)
{
    if (status == SQUELCH_OPEN)
        return "Open";
    else if (status == SQUELCH_CLOSED)
        return "Clsd";
    else
        return "Invalid";
}

TX_Params::TX_Params() : ptt_status(INVALID_VALUE), forward_power(INVALID_FLOAT), reverse_power(INVALID_FLOAT), vswr(INVALID_FLOAT)
{}

std::string TX_Params::ptt_string() const
{
    return ::ptt_string(ptt_status);
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
    return ::squelch_string(squelch_status);
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

CM300_Radio::~CM300_Radio()
{
    if (sk)
        delete sk;
}

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

Radio_Telnet::Radio_Telnet()
    : _logging(true),
      _simulate_radios(false),
      _simulation_period(2000),
      _simulated_high_vswr_period_count(30),
      _simulated_random_sq_period_count(5),
      _ip_lb(10),
      _ip_ub(13),
      _max_retry_count(10),
      _conn_timeout(0, 500000),
      _cur_cmd(INVALID_VALUE),
      commands{cmd::str::ID, cmd::str::FREQ, cmd::str::MEAS, cmd::str::RSTAT},
      complete_scans(0)
{
    resp_len = strlen(RESPONSE_COMPLETE_STR);
}

Radio_Telnet::~Radio_Telnet()
{}

void parse_item_groupj(const nlohmann::json & source, const std::string & name, Logger_Entry * le)
{
    nlohmann::json option_obj;
    try
    {
        if (fill_param_if_found(source, name, &option_obj))
        {
            std::string str;
            Log_Option_Group log;
            try
            {
                log.title.enabled = fill_param_if_found(option_obj, "title", &log.title.val);
            }
            catch (nlohmann::detail::exception & e)
            {
                elog("Error for title is in parent json object {}", name);
            }

            if (name == "ptt_status" || name == "squelch_status")
            {
                try
                {
                    log.equal.enabled = fill_param_if_found(option_obj, "equal", &str);
                }
                catch (nlohmann::detail::exception & e)
                {
                    elog("Error for equal is in parent json object {}", name);
                }

                log.change.enabled = MAP_CONTAINS(option_obj, "change");
                if (log.equal.enabled)
                {
                    util::to_lower(str);
                    if (str.find("off") != std::string::npos)
                        log.equal.val |= PTT_OFF;
                    if (str.find("loc") != std::string::npos)
                        log.equal.val |= PTT_LOCAL;
                    if (str.find("rem") != std::string::npos)
                        log.equal.val |= PTT_REMOTE;
                    if (str.find("test") != std::string::npos)
                        log.equal.val |= PTT_TEST_RF;
                    if (str.find("open") != std::string::npos)
                        log.equal.val |= SQUELCH_OPEN;
                    if (str.find("clos") != std::string::npos)
                        log.equal.val |= SQUELCH_CLOSED;
                }
            }
            else
            {
                try
                {
                    log.change.enabled = fill_param_if_found(option_obj, "change", &log.change.val);
                }
                catch (nlohmann::detail::exception & e)
                {
                    elog("Error for change is in parent json object {}", name);
                }

                if (log.change.enabled)
                    log.change.val = std::abs(log.change.val);

                try
                {
                    log.percent_change.enabled = fill_param_if_found(option_obj, "percent_change", &log.percent_change.val);
                    if (log.percent_change.enabled)
                        log.percent_change.val = std::abs(log.percent_change.val);
                }
                catch (nlohmann::detail::exception & e)
                {
                    elog("Error for percent_change is in parent json object {}", name);
                }

                try
                {
                    log.less_than.enabled = fill_param_if_found(option_obj, "less_than", &log.less_than.val);
                }
                catch (nlohmann::detail::exception & e)
                {
                    elog("Error for less_than is in parent json object {}", name);
                }

                try
                {
                    log.greater_than.enabled = fill_param_if_found(option_obj, "greater_than", &log.greater_than.val);
                }
                catch (nlohmann::detail::exception & e)
                {
                    elog("Error for greater_than is in parent json object {}", name);
                }
            }
            le->loptions.item_options[name] = log;
        }
    }
    catch (nlohmann::detail::exception & e)
    {
        // skip it
    }
}

void Radio_Telnet::_set_options_from_config_file(Config_File * cfg)
{
    nlohmann::json obj;
    static bool added_status_log = false;

    cfg->fill_param_if_found("logging_enabled", &_logging);
    cfg->fill_param_if_found("simulate_radios", &_simulate_radios);
    cfg->fill_param_if_found("simulation_period", &_simulation_period);
    cfg->fill_param_if_found("simulation_high_vswr_period_count", &_simulated_high_vswr_period_count);
    cfg->fill_param_if_found("simulation_random_squelch_break_period_count", &_simulated_random_sq_period_count);
    cfg->fill_param_if_found("ip_lower_bound", &_ip_lb);
    cfg->fill_param_if_found("ip_upper_bound", &_ip_ub);
    cfg->fill_param_if_found("loggers", &obj);

    // Now look in the sub obj for the per logger info
    auto iter = obj.begin();
    while (iter != obj.end())
    {
        Logger_Entry le;
        le.name = iter.key();
        try
        {
            fill_param_if_found(*iter, "dir_path", &le.loptions.dir_path);
        }
        catch (nlohmann::detail::exception & e)
        {
            elog("Error for dir_path is in parent json object {}", iter.key());
        }
        if (!le.loptions.dir_path.empty())
        {
            if (mkdir(le.loptions.dir_path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0)
            {
                ilog("Created dir {} for log {}", le.loptions.dir_path, le.name);
            }
        }

        le._backup_log_dir = util::get_home_dir() + "/csvlogs";
        if (mkdir(le._backup_log_dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0)
        {
            ilog("Created backup log dir {} for logger {}", le._backup_log_dir, le.name);
        }

        try
        {
            fill_param_if_found(*iter, "period", &le.loptions.period);
        }
        catch (nlohmann::detail::exception & e)
        {
            elog("Error for period is in parent json object {}", iter.key());
        }

        try
        {
            // Only one log entry can log additionally to the status log
            if (!added_status_log)
                added_status_log =
                    fill_param_if_found(*iter, "log_changes_to_status", &le.loptions.log_changes_to_status) && le.loptions.log_changes_to_status;
        }
        catch (nlohmann::detail::exception & e)
        {
            elog("Error for log_changes_to_status is in parent json object {}", iter.key());
        }

        parse_item_groupj(*iter, "ptt_status", &le);
        parse_item_groupj(*iter, "squelch_status", &le);
        parse_item_groupj(*iter, "forward_power", &le);
        parse_item_groupj(*iter, "reverse_power", &le);
        parse_item_groupj(*iter, "vswr", &le);
        parse_item_groupj(*iter, "agc", &le);
        parse_item_groupj(*iter, "line_level", &le);

        _loggers[iter.key()] = le;
        ++iter;
    }
}

void Radio_Telnet::init(Config_File * config)
{
    Subsystem::init(config);
    _set_options_from_config_file(config);

    int8_t size = (_ip_ub - _ip_lb + 1);
    int64_t arg = 0;

    if (_simulate_radios)
    {
        int radio_set_count = size / 2;
        int vcount = radio_set_count / 2 + radio_set_count % 2;
        int ucount = radio_set_count - vcount;
        ilog("Simulate radios set to true in config - building simulation with {} VHF and {} UHF radio sets", vcount, ucount);
        ilog("Simulation period set to {}ms, random squelch breaks every {} periods, and high VSWR on TX every {} periods",
             _simulation_period,
             _simulated_random_sq_period_count,
             _simulated_high_vswr_period_count);
        create_simulated_radio_set(_radios, vcount, ucount);
    }
    else
    {
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
    // for (int i = 0; i < _radios.size(); ++i)
    //     delete _radios[i].sk;
    _radios.clear();
}

bool _check_status_option(const Logger_Options & le, const std::string & param_name, int32_t cur_status, int32_t prev_status, const CM300_Radio * rad)
{
    auto iter = le.item_options.find(param_name);
    if (iter != le.item_options.end())
    {
        bool cond_change = (iter->second.change.enabled && (cur_status != prev_status));

        if (le.log_changes_to_status && cond_change)
        {
            std::string pm(param_name);
            if (iter->second.title.enabled && !iter->second.title.val.empty())
                pm = iter->second.title.val;
            std::string prev_string, cur_string;
            if (rad->radio_type() == TX_STR)
            {
                prev_string = ptt_string(prev_status);
                cur_string = ptt_string(cur_status);
            }
            else
            {
                prev_string = squelch_string(prev_status);
                cur_string = squelch_string(cur_status);
            }

            ilog("{} {} ({}): {} changed to {} (was {})", rad->freq_mhz, rad->radio_type(), rad->serial, pm, cur_string, prev_string);
        }

        bool cond_equal = (iter->second.equal.enabled && BITS_SET(iter->second.equal.val, cur_status));
        return cond_change || cond_equal;
    }
    return false;
}

bool _check_float_option(const Logger_Options & le, const std::string & param_name, float cur_val, float prev_val, const CM300_Radio * rad)
{
    auto iter = le.item_options.find(param_name);
    if (iter != le.item_options.end())
    {
        double change = std::abs(cur_val - prev_val);
        double percent_change = change;
        if (!DEQUALS(percent_change, 0.0, EPS))
            percent_change = (change / std::abs(cur_val));
        percent_change *= 100.0;

        bool cond_change = (iter->second.change.enabled && (change > iter->second.change.val));
        bool cond_percent_change = (iter->second.percent_change.enabled && (percent_change > iter->second.percent_change.val));

        if (le.log_changes_to_status && (cond_change || cond_percent_change))
        {
            std::string pm(param_name);
            if (iter->second.title.enabled && !iter->second.title.val.empty())
                pm = iter->second.title.val;
            ilog("{} {} ({}): {} changed over log threshold to {} (was {} - change of {}%)",
                 rad->freq_mhz,
                 rad->radio_type(),
                 rad->serial,
                 pm,
                 cur_val,
                 prev_val,
                 percent_change);
        }

        bool cond_less_than = (iter->second.less_than.enabled && (cur_val < iter->second.less_than.val));
        bool cond_greater_than = (iter->second.greater_than.enabled && (cur_val > iter->second.greater_than.val));

        bool cond_less_greater = cond_less_than || cond_greater_than;

        if (iter->second.less_than.enabled && iter->second.greater_than.enabled && (iter->second.less_than.val > iter->second.greater_than.val))
            cond_less_greater = cond_less_than && cond_greater_than;

        return cond_change || cond_percent_change || cond_less_greater;
    }
    return false;
}

void Logger_Entry::update_and_log_if_needed(const std::vector<CM300_Radio> & radios)
{
    ms_counter += edm.sys_timer()->dt();
    bool should_log = false;

    if (ms_counter >= loptions.period)
    {
        ms_counter = 0;

        for (int rind = 0; rind < radios.size(); ++rind)
        {
            CM300_Radio * prev = &prev_state[rind];
            const CM300_Radio * cur = &radios[rind];
            bool is_tx = (cur->radio_type() == TX_STR);

            // Always log on serial change or freq change
            should_log = should_log || (cur->serial != prev->serial) || !DEQUALS(cur->freq_mhz, prev->freq_mhz, EPS);

            if (loptions.log_changes_to_status)
            {
                if (is_tx)
                {
                    should_log = _check_status_option(loptions, "ptt_status", cur->tx.ptt_status, prev->tx.ptt_status, cur) || should_log;
                    should_log = _check_float_option(loptions, "forward_power", cur->tx.forward_power, prev->tx.forward_power, cur) || should_log;
                    should_log = _check_float_option(loptions, "reverse_power", cur->tx.reverse_power, prev->tx.reverse_power, cur) || should_log;
                    should_log = _check_float_option(loptions, "vswr", cur->tx.vswr, prev->tx.vswr, cur) || should_log;
                }
                else
                {
                    should_log = _check_status_option(loptions, "squelch_status", cur->rx.squelch_status, prev->rx.squelch_status, cur) || should_log;
                    should_log = _check_float_option(loptions, "agc", cur->rx.agc, prev->rx.agc, cur) || should_log;
                    should_log = _check_float_option(loptions, "line_level", cur->rx.line_level, prev->rx.line_level, cur) || should_log;
                }
            }
            else
            {
                if (is_tx)
                {
                    should_log = should_log || _check_status_option(loptions, "ptt_status", cur->tx.ptt_status, prev->tx.ptt_status, cur);
                    should_log = should_log || _check_float_option(loptions, "forward_power", cur->tx.forward_power, prev->tx.forward_power, cur);
                    should_log = should_log || _check_float_option(loptions, "reverse_power", cur->tx.reverse_power, prev->tx.reverse_power, cur);
                    should_log = should_log || _check_float_option(loptions, "vswr", cur->tx.vswr, prev->tx.vswr, cur);
                }
                else
                {
                    should_log = should_log || _check_status_option(loptions, "squelch_status", cur->rx.squelch_status, prev->rx.squelch_status, cur);
                    should_log = should_log || _check_float_option(loptions, "agc", cur->rx.agc, prev->rx.agc, cur);
                    should_log = should_log || _check_float_option(loptions, "line_level", cur->rx.line_level, prev->rx.line_level, cur);
                }
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

void Radio_Telnet::_simulated_radios_update()
{
    static double counter = 0;
    static int counter_high_vswr = 0;
    static int counter_rx_squelch = 0;

    counter += edm.sys_timer()->dt();

    static size_t tx_ind = 1;
    static uint8_t CUR_PTT_STATE = PTT_LOCAL;
    static size_t rx_squelch_break_ind = -1;

    // Simulate PTT on cycling radios (RX and TX) switching the radio every 2 seconds
    // Also generate a high VSWR on a transmitter every minute
    // Finally, generate random radio squelch breaks every 10 seconds - should always be on a radio that is not currently keyed
    if (counter > _simulation_period)
    {
        ++counter_rx_squelch;
        // If the corresponding transmitter is not keyed, then close the randomly generated squelch break previously opened
        if (rx_squelch_break_ind != -1 && ((rx_squelch_break_ind != (tx_ind - 1)) || (_radios.size() == 1)))
        {
            _radios[rx_squelch_break_ind].rx.squelch_status = SQUELCH_CLOSED;
            _radios[rx_squelch_break_ind].rx.agc = 1.61;
            _radios[rx_squelch_break_ind].rx.line_level = -46.0;
            rx_squelch_break_ind = -1;
        }

        // Every 10 seconds generate a random squelch break
        if (counter_rx_squelch == _simulated_random_sq_period_count)
        {
            // Generate a random receiver squelch break index
            do
            {
                rx_squelch_break_ind = (rand() % _radios.size() / 2) * 2;
            } while ((rx_squelch_break_ind == (tx_ind - 1)) && (_radios.size() > 1));
            _radios[rx_squelch_break_ind].rx.squelch_status = SQUELCH_OPEN;
            _radios[rx_squelch_break_ind].rx.agc = 2.2;
            _radios[rx_squelch_break_ind].rx.line_level = -8.0;
            counter_rx_squelch = 0;
        }

        if (tx_ind < _radios.size())
        {
            CM300_Radio * cur = &_radios[tx_ind];
            CM300_Radio * cur_rx = cur - 1;

            int prev_tx_index = tx_ind - 2;
            if (prev_tx_index < 0)
                prev_tx_index = _radios.size() - 1;

            cur->tx.ptt_status = CUR_PTT_STATE;
            cur->tx.forward_power = 12;
            cur->tx.vswr = 1.0;
            cur_rx->rx.squelch_status = SQUELCH_OPEN;
            cur_rx->rx.line_level = -8;
            cur_rx->rx.agc = 2.6;

            if (prev_tx_index != tx_ind)
            {
                CM300_Radio * prev_tx = &_radios[prev_tx_index];
                CM300_Radio * prev_rx = prev_tx - 1;

                prev_tx->tx.ptt_status = PTT_OFF;
                prev_tx->tx.forward_power = 0;
                prev_tx->tx.vswr = -1.0;

                // Only turn off the previous RX squelch if it isn't the random one we opened squelch on this round
                if ((prev_tx_index - 1) != rx_squelch_break_ind)
                {
                    prev_rx->rx.squelch_status = SQUELCH_CLOSED;
                    prev_rx->rx.line_level = -46.0;
                    prev_rx->rx.agc = 1.61;
                }
            }

            // Give a TX high VSWR once every minute
            ++counter_high_vswr;
            if (counter_high_vswr == _simulated_high_vswr_period_count)
            {
                cur->tx.forward_power = 12.0;
                cur->tx.reverse_power = 2.2041;
                cur->tx.vswr = 2.5;
                counter_high_vswr = 0;
            }

            tx_ind += 2;
            if (tx_ind > _radios.size())
            {
                tx_ind = 1;
                CUR_PTT_STATE *= 2;
                if (CUR_PTT_STATE > PTT_TEST_RF)
                    CUR_PTT_STATE = PTT_LOCAL;
            }
        }

        counter = 0;
    }
}

void Radio_Telnet::update()
{
    static std::set<CM300_Radio *> initialized_radios;
    static bool all_radios_init = false;

    bool complete_scan = true;

    auto iter = _radios.begin();
    while (iter != _radios.end())
    {
        CM300_Radio prev = *iter;

        _update(&(*iter));
        _update_closed(&(*iter));

        if (iter->initialized() && initialized_radios.insert(&(*iter)).second)
        {
            if (iter->sk)
            {
                ilog("Radio at {} initialized:\n{}", iter->sk->get_ip(), iter->to_string());
            }
            else
            {
                ilog("Radio (no socket) initialized:\n{}", iter->to_string());
            }
        }

        complete_scan = complete_scan && (iter->complete_scan_count > complete_scans);
        ++iter;
    }

    if (_simulate_radios)
        _simulated_radios_update();

    if (complete_scan)
    {
        if (all_radios_init && _logging)
        {
            auto liter = _loggers.begin();
            while (liter != _loggers.end())
            {
                liter->second.update_and_log_if_needed(_radios);
                ++liter;
            }
        }
        ++complete_scans;
    }

    if (!all_radios_init && initialized_radios.size() == _radios.size())
    {
        ilog("All radios initialized");
        all_radios_init = true;

        // Setup the loggers prev state to now!
        auto liter = _loggers.begin();
        while (liter != _loggers.end())
        {
            liter->second.prev_state = _radios;
            liter->second.write_headers_to_file();
            liter->second.write_radio_data_to_file();
            ++liter;
        }
    }
}

void _handle_option_header(const Logger_Options & options, std::vector<std::string> & cur_row, const std::string & param)
{
    auto iter = options.item_options.find(param);
    if (iter != options.item_options.end())
    {
        std::string title(param);
        if (iter->second.title.enabled)
            title = iter->second.title.val;
        cur_row.push_back(title);
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
            _handle_option_header(loptions, cur_row, "ptt_status");
            _handle_option_header(loptions, cur_row, "forward_power");
            _handle_option_header(loptions, cur_row, "reverse_power");
            _handle_option_header(loptions, cur_row, "vswr");
        }
        else if (rad->radio_type() == RX_STR)
        {
            _handle_option_header(loptions, cur_row, "squelch_status");
            _handle_option_header(loptions, cur_row, "agc");
            _handle_option_header(loptions, cur_row, "line_level");
        }
        else
        {
            wlog("Unknown Radio Type (serial: {} freq: {})", rad->serial, rad->freq_mhz);
        }

        for (int i = 0; i < cur_row.size(); ++i)
        {
            if (i == 0)
                first_row += NUM_2_STR(rad->freq_mhz, 3) + " " + rad->radio_range() + " " + rad->radio_type() + " (" + rad->serial + ")";
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
            if (MAP_CONTAINS(loptions.item_options, "ptt_status"))
                cur_row.push_back(rad->tx.ptt_string());
            if (MAP_CONTAINS(loptions.item_options, "forward_power"))
                cur_row.push_back(NUM_2_STR(rad->tx.forward_power, 2));
            if (MAP_CONTAINS(loptions.item_options, "reverse_power"))
                cur_row.push_back(NUM_2_STR(rad->tx.reverse_power, 2));
            if (MAP_CONTAINS(loptions.item_options, "vswr"))
                cur_row.push_back(NUM_2_STR(rad->tx.vswr, 2));
        }
        else if (rad->radio_type() == RX_STR)
        {
            if (MAP_CONTAINS(loptions.item_options, "squelch_status"))
                cur_row.push_back(rad->rx.squelch_string());
            if (MAP_CONTAINS(loptions.item_options, "agc"))
                cur_row.push_back(NUM_2_STR(rad->rx.agc, 2));
            if (MAP_CONTAINS(loptions.item_options, "line_level"))
                cur_row.push_back(NUM_2_STR(rad->rx.line_level, 2));
        }
        for (int i = 0; i < cur_row.size(); ++i)
            row += cur_row[i] + ",";
    }

    if (!row.empty())
    {
        row.pop_back();
        row = util::get_current_time_string() + "," + NUM_2_STR(edm.sys_timer()->elapsed() / 1000.0, 2) + "," + row;
    }
    return row;
}

std::string Logger_Entry::get_fname()
{
    time_t t = time(nullptr);
    tm * ltm = localtime(&t);

    std::string fname = name + " [" + util::formatted_date(ltm) + " at " + std::to_string(ltm->tm_hour * 100) + "].csv";
    if (!loptions.dir_path.empty())
    {
        if (loptions.dir_path.back() != '/')
            fname = "/" + fname;
        fname = loptions.dir_path + fname;
    }
    return fname;
}

bool Logger_Entry::write_headers_to_file()
{
    std::ofstream output;
    std::string fname = get_fname();

    if (util::path_exists(fname))
    {
        std::string new_name = fname.substr(0, fname.size() - 4) + " (Stopped " + util::get_current_time_string() + ").csv";
        if (std::rename(fname.c_str(), new_name.c_str()) == 0)
        {
            ilog("Renaming {} to {} as the logger has been restarted (renamed file is from previous execution)", fname, new_name);
        }
        else
        {
            wlog("Could not rename {} to {}: {} - the logger was restarted so this file will be overwritten", fname, new_name, strerror(errno));
        }
    }

    output.open(fname, std::ios::out | std::ios::trunc);
    if (output.is_open())
    {
        ilog("Successfully opened {} for logging", fname);
        output << get_header() << "\n";
        output.close();
        return true;
    }
    else
    {
        ilog("Could not open {}: {}", fname, strerror(errno));
        if (!loptions.dir_path.empty())
        {
            ilog("Trying to open file in {} instead of {}", _backup_log_dir, loptions.dir_path);
            std::string saved = loptions.dir_path;
            loptions.dir_path = _backup_log_dir;
            bool result = write_headers_to_file();
            loptions.dir_path = saved;
            return result;
        }
    }
    return false;
}

bool Logger_Entry::write_radio_data_to_file()
{
    std::ofstream output;
    std::string fname = get_fname();
    output.open(fname, std::ios::out | std::ios::app);
    if (output.is_open())
    {
        output << get_row() << "\n";
        output.close();
        return true;
    }
    else if (!loptions.dir_path.empty())
    {
        std::string saved = loptions.dir_path;
        loptions.dir_path = _backup_log_dir;
        bool result = write_radio_data_to_file();
        loptions.dir_path = saved;
        return result;
    }
    return false;
}

void Radio_Telnet::_update(CM300_Radio * radio)
{
    if (!radio->sk)
    {
        ++radio->complete_scan_count;
        return;
    }

    uint32_t cnt = radio->sk->read(radio->response_buffer + radio->buffer_offset, BUFFER_SIZE - radio->buffer_offset);
    radio->buffer_offset += cnt; // buffer offset holds the new size

    if (radio->buffer_offset >= resp_len)
    {
        if (strncmp((char *)radio->response_buffer + (radio->buffer_offset - resp_len), RESPONSE_COMPLETE_STR, resp_len) == 0)
        {
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

bool Radio_Telnet::logging_enabled() const
{
    return _logging;
}

void Radio_Telnet::set_ip_lower_bound(int8_t ip_lb)
{
    _ip_lb = ip_lb;
}

int8_t Radio_Telnet::get_ip_lower_bound() const
{
    return _ip_lb;
}

void Radio_Telnet::set_ip_upper_bound(int8_t ip_ub)
{
    _ip_ub = ip_ub;
}

int8_t Radio_Telnet::get_ip_upper_bound() const
{
    return _ip_ub;
}

const char * Radio_Telnet::typestr()
{
    return TypeString();
}

const char * Radio_Telnet::TypeString()
{
    return "Radio_Telnet";
}
