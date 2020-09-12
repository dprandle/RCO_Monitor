#pragma once

#include <edsystem.h>
#include <vector>

class edtimer;
class edgpio;

const double PROCESS_TIMEOUT = 0.083; // 83 microseconds for 100 points every half cycle (60 Hz)
const int LIGHT_COUNT = 6;
const double LIGHT_MAX = 90.0;
const double LIGHT_MIN = 30.0;
const double SERVER_LIGHT_MAX = 100.0;

struct Light_Info
{
    std::vector<uint8_t> ms_data;
};

struct Automation_Data
{
    union
    {
        uint32_t sample_cnt;
        uint8_t data[4];
    };
    Light_Info lights[LIGHT_COUNT];
};

class edlight_system : public edsystem
{
  public:
    edlight_system();

    ~edlight_system();

    void play();

    void pause();

    void stop();

    void init();

    void release();

    void update();

    void set_automation_data(const Automation_Data & data);

    void sync_input();

    void process(edtimer * timer);

    std::string typestr();

    static std::string TypeString();

  private:
    uint8_t translate_level_(uint8_t lvl);

    int tmp;
    int counter;
    edgpio * sync_;
    std::vector<edgpio *> light_pins_;
    edtimer * play_timer_;
    edtimer * process_timer;
    Automation_Data light_data_;
    uint8_t current_frame_[LIGHT_COUNT];
};
