#include <version.h>
#include <date.h>

#include <vector>
#include <string>
#include <sstream>
#include <iostream>

// 2024-03-23 - deadbeefbadc0ffee (v6.9)
std::string Version::string() const
{
    std::stringstream ss;
    ss << date.get_date() << " - " << id << (tag.empty() ? "" : " ("+tag+")");
    return ss.str();
}

bool Version::operator==(const Version &other)
{
    return id == other.id;
}

bool Version::operator!=(const Version &other)
{
    return id != other.id;
}
