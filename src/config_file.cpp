#include <fstream>

#include "utility.h"
#include "json.h"
#include "config_file.h"
#include "logger.h"

using namespace nlohmann;

Config_File::Config_File()
{}

Config_File::~Config_File()
{}

bool Config_File::load(const std::string & fname)
{
    std::string input_txt;
    if (!util::read_file_contents_to_string(fname, input_txt))
        return false;
    _strip_comments(input_txt);
    _strip_empty_lines(input_txt);
    _config_obj = json::parse(input_txt);
    return true;
}

std::string Config_File::dump()
{
    return _config_obj.dump(4);
}

void Config_File::_strip_comments(std::string & str)
{
    size_t pos = str.find("//");
    while (pos != std::string::npos)
    {
        size_t nlpos = str.find('\n', pos);
        str.erase(pos, nlpos-pos);
        pos = str.find("//");
    }
}

void Config_File::_strip_empty_lines(std::string & str)
{
    size_t pos = 0;
    while (pos < str.size())
    {
        if (pos)
            ++pos;
        
        size_t nlpos = str.find('\n', pos);
        if (str.find_first_not_of(' ', pos) == nlpos)
        {
            if (nlpos == std::string::npos)
                str.erase(pos,nlpos);
            else
                str.erase(pos, nlpos-pos+1);
        }
        else
        {
            pos = nlpos;
        }
    }
}