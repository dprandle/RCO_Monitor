#pragma once

class Subsystem
{
  public:
    Subsystem();

    virtual ~Subsystem();

    virtual void init();

    virtual void release();

    virtual void update();

    virtual const char * typestr() = 0;
};