#pragma once
#include <string>

class edsystem
{
  public:
    edsystem()
    {}

    virtual ~edsystem()
    {}

    virtual void init()
    {}

    virtual void release()
    {}

    virtual void update()
    {}

    virtual std::string typestr() = 0;
};