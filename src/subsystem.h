#pragma once

class Config_File;

class Subsystem
{
  public:
    Subsystem();

    virtual ~Subsystem();

    virtual void init(Config_File * config);

    virtual void release();

    virtual void update();

    virtual const char * typestr() = 0;
};