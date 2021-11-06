#include "subsystem.h"
#include "logger.h"

Subsystem::Subsystem()
{}

Subsystem::~Subsystem()
{}

void Subsystem::init(Config_File * config)
{
    ilog("Initializing system {}", typestr());
}

void Subsystem::release()
{
    ilog("Releasing system {}", typestr());
}

void Subsystem::update()
{}
