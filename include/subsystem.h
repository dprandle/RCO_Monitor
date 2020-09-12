#pragma once
#include <string>

class Subsystem
{
  public:
    Subsystem()
    {}

    virtual ~Subsystem()
    {}

    virtual void init()
    {}

    virtual void release()
    {}

    virtual void update()
    {}

    virtual std::string typestr() = 0;
};