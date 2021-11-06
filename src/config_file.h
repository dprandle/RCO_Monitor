#pragma once

#include <string>
#include "json.h"
#include "logger.h"

template<class T>
bool fill_param_if_found(const nlohmann::json & obj, const std::string & name, T * param)
{
    auto fiter = obj.find(name);
    if (fiter != obj.end())
    {
        try
        {
            *param = fiter->get<T>();
        }
        catch (nlohmann::detail::exception & e)
        {
            elog("Error in reading config file param {}: {} - fix the config file this param is being ignored", name, e.what());
            throw e;
        }
        return true;
    }
    return false;
}

class Config_File
{
  public:
    Config_File();
    ~Config_File();

    bool load(const std::string & fname);

    template<class T>
    bool fill_param_if_found(const std::string & name, T * param)
    {
        try
        {
            return ::fill_param_if_found(_config_obj, name, param);
        }
        catch(nlohmann::detail::exception & e)
        {
            return false;
        }
    }

    std::string dump();

  private:
    void _strip_comments(std::string & str);
    void _strip_empty_lines(std::string & str);
    nlohmann::json _config_obj;
};