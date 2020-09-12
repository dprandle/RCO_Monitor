#pragma once

#include "subsystem.h"

class RCE_Serial_Comm : public Subsystem
{
  public:
    RCE_Serial_Comm();

    virtual ~RCE_Serial_Comm();

    virtual void init();

    virtual void release();

    virtual void update();

    virtual std::string typestr();

    static std::string TypeString()
    {
        return "RCE_Serial_Comm";
    }

  private:
};
