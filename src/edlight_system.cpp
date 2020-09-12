#include <edlight_system.h>
#include <edlogger.h>
#include <edtimer.h>
#include <edgpio.h>

edlight_system::edlight_system()
    : tmp(0), counter(0), sync_(nullptr), light_pins_(), play_timer_(new edtimer), process_timer(new edtimer), light_data_(), current_frame_()
{}

edlight_system::~edlight_system()
{
    delete process_timer;
    delete play_timer_;
}

void edlight_system::init()
{
    using namespace std::placeholders;
    std::function<void(edtimer *)> cb_func = std::bind(&edlight_system::process, this, _1);
    process_timer->set_callback(cb_func);
    process_timer->set_callback_mode(edtimer::continous_shot);
    process_timer->set_callback_delay(PROCESS_TIMEOUT);
    process_timer->start();

    play_timer_->set_callback_mode(edtimer::no_shot);

    sync_ = new edgpio(GPIO_15);

    gpio_error_state st = sync_->get_and_clear_error();
    if (st.gp_code != gpio_no_error)
    {
        elog("GPIO had error in opening pin: {}", sync_->error_string(st));
        delete sync_;
        sync_ = nullptr;
    }
    else
    {
        ilog("Successfully initialized sync pin on GPIO {}", sync_->pin_num());
    }

    sync_->set_direction(gpio_dir_in);
    st = sync_->get_and_clear_error();
    if (st.gp_code != gpio_no_error)
    {
        elog("GPIO had error in setting pin direction: {}", sync_->error_string(st));
        delete sync_;
        sync_ = nullptr;
    }
    else
    {
        ilog("Successfully set sync pin direction to input ({})", gpio_dir_in);
    }

    std::function<void()> isr_func = std::bind(&edlight_system::sync_input, this);
    int ret = sync_->set_isr(gpio_edge_rising, isr_func);
    st = sync_->get_and_clear_error();
    if (ret == -1 || st.gp_code != gpio_no_error)
    {
        elog("GPIO had error in setting up ISR: {}", sync_->error_string(st));
        delete sync_;
        sync_ = nullptr;
    }
    else
    {
        ilog("Successfully set sync pin ISR");
    }

    for (int i = 0; i < LIGHT_COUNT; ++i)
    {
        edgpio * pin = nullptr;
        pin = new edgpio(GPIO_44 + i);

        st = pin->get_and_clear_error();
        if (st.gp_code != gpio_no_error)
        {
            elog("GPIO had error in opening pin: {}", pin->error_string(st));
            delete pin;
            continue;
        }
        else
        {
            ilog("Successfully initialized pin for light strand {} on GPIO {}", i, pin->pin_num());
        }

        pin->set_direction(gpio_dir_out);
        st = pin->get_and_clear_error();
        if (st.gp_code != gpio_no_error)
        {
            elog("GPIO had error in setting pin direction: {}", pin->error_string(st));
            delete pin;
            continue;
        }
        else
        {
            ilog("Successfully set sync pin direction to output ({})", gpio_dir_out);
        }
        light_pins_.push_back(pin);
    }
}

void edlight_system::play()
{
    play_timer_->start();
    ilog("Starting light show!");
}

void edlight_system::pause()
{
    play_timer_->pause();
    ilog("Pausing light show!");
}

void edlight_system::stop()
{
    play_timer_->stop();
    ilog("Stopping light show!");
}

void edlight_system::release()
{}

void edlight_system::set_automation_data(const Automation_Data & data)
{
    light_data_.sample_cnt = data.sample_cnt;
    for (int i = 0; i < LIGHT_COUNT; ++i)
        light_data_.lights[i].ms_data = data.lights[i].ms_data;
}

void edlight_system::sync_input()
{
    for (int i = 0; i < LIGHT_COUNT; ++i)
    {
        if (!play_timer_->paused() && !play_timer_->running())
            light_pins_[i]->write_pin(1);
        else
            light_pins_[i]->write_pin(0);
    }

    if (light_data_.sample_cnt == 0)
        return;

    uint32_t cur_ms = play_timer_->elapsed();
    if (cur_ms >= light_data_.sample_cnt)
    {
        play_timer_->stop();
        play_timer_->start();
        cur_ms = 0;
    }

    for (int i = 0; i < LIGHT_COUNT; ++i)
    {
        current_frame_[i] = translate_level_(light_data_.lights[i].ms_data[cur_ms]);
    }

    counter = 0;
}

void edlight_system::update()
{
    process_timer->update();
    play_timer_->update();
    sync_->update();
    for (int i = 0; i < light_pins_.size(); ++i)
        light_pins_[i]->update();
}

uint8_t edlight_system::translate_level_(uint8_t lvl)
{
    double scale = 1 - double(lvl) / SERVER_LIGHT_MAX;
    double new_val = scale * (LIGHT_MAX - LIGHT_MIN) + LIGHT_MIN;
    return uint8_t(new_val);
}

void edlight_system::process(edtimer * timer)
{
    ++counter;
    for (int i = 0; i < LIGHT_COUNT; ++i)
    {
        if (counter == current_frame_[i])
        {
            light_pins_[i]->write_pin(1);
        }
    }
}

std::string edlight_system::typestr()
{
    return TypeString();
}

std::string edlight_system::TypeString()
{
    return "edlight_system";
}
