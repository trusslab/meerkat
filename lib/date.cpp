#include <date.h>

#include <string>
#include <cctype>
#include <sstream>

bool Date::validate_date(unsigned int y, unsigned int m, unsigned int d) const
{
    if (y == 0 || m > 12 || m == 0)
        return false;

    if (d > days_in_month(m, y) || d == 0)
        return false;

    return true;
}

unsigned int Date::days_in_month(unsigned int m, unsigned int y) const
{
    unsigned int days = 30;

    days += 1 * (m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12);
    days -= 1 * (m == 2) + 1 * (m == 2 && y % 4 != 0);

    return days;
}

int Date::day_count(const Date & date) const
{
    return date.year*365 + (date.year - 1)/4 + days_to_month(date.month, date.year) + date.day;
}

unsigned int Date::days_to_month(unsigned int m, unsigned int y) const
{
    int total = 0;

    switch(m)
    {
    case 12:
        total += 30;
    case 11:
        total += 31;
    case 10:
        total += 30;
    case 9:
        total += 31;
    case 8:
        total += 31;
    case 7:
        total += 30;
    case 6:
        total += 31;
    case 5:
        total += 30;
    case 4:
        total += 31;
    case 3:
        total += 28 + (y % 4 == 0);
    case 2:
        total += 31;
    case 1:
        break;
    default:
        break;
    }
    return total;
}

Date::Date(const std::string & s)
{
    if (!set_date(s))
    {
        day = 0;
        month = 0;
        year = 0;
        delim = '-';
    }

    return;
}

Date::Date(unsigned int y, unsigned int m, unsigned int d, char de)
{
    if (!set_date(y, m, d))
        return;
    else
        set_delim(de);

    return;
}

Date::Date(const Date & other)
    : Date(other.year, other.month, other.day, other.delim)
{
    return;
}

bool Date::set_date(const std::string & s)
{
    bool ok = true;
    char de = '-';

    if (s.size() != 10)
        return false;

    int d = 0, m = 0, y = 0;

    for (int i = 0; i < 4 && ok; i++)
    {
        if (!isdigit(s.at(i)))
            ok = false;
        else
            y = y * 10 + (s.at(i) - '0');
    }

    if (ispunct(s.at(4)))
        de = s.at(4);
    else
        ok = false;

    for (int i = 5; i < 7 && ok; i++)
    {
        if (!isdigit(s.at(i)))
            ok = false;
        else
            m = m * 10 + (s.at(i) - '0');
    }
    ok = (s.at(7) == de) ? ok : false;

    for (int i = 8; i < 10 && ok; i++)
    {
        if (!isdigit(s.at(i)))
            ok = false;
        else
            d = d * 10 + (s.at(i) - '0');
    }

    if (set_date(y, m, d))
        set_delim(de);
    else
        ok = false;

    return ok;
}

bool Date::set_date(unsigned int y, unsigned int m, unsigned int d)
{
    if (!validate_date(y, m, d))
        return false;

    day = d;
    month = m;
    year = y;
    return true;
}

bool Date::set_delim(char d)
{
    if (!ispunct(d))
        return false;

    delim = d;
    return true;
}

unsigned int Date::get_day() const
{
    return day;
}

unsigned int Date::get_month() const
{
    return month;
}

unsigned int Date::get_year() const
{
    return year;
}

std::string Date::get_date() const
{
    std::stringstream ss;
    ss << (year < 1000 ? "0" : "") << (year < 100 ? "0" : "") << (year < 10 ? "0" : "")  << year 
        << delim 
        << (month < 10 ? "0" : "")  << month 
        << delim 
        << (day < 10 ? "0" : "") << day;

    std::string date;
    ss >> date;
    return date;
}

Date Date::inc()
{
    Date date(*this);

    date.day++;
    if (date.day > days_in_month(date.month, date.year))
    {
        date.day = 1;
        date.month++;
        if (date.month > 12);
        {
            date.month = 1;
            date.year++;
        }
    }

    return date;
}

Date Date::dec()
{
    Date date(*this);

    date.day--;
    if (date.day < 1)
    {
        date.month--;
        if (date.month < 1)
        {
            date.month = 12;
            date.year--;
        }
        date.day = days_in_month(date.month, date.year);
    }

    return date;
}

Date Date::date_plus_days(unsigned int n)
{
    Date date(*this);

    date.day += n;
    while (date.day > days_in_month(date.month, date.year))
    {
        date.day -= days_in_month(date.month, date.year);
        date.month++;
        if (date.month > 12)
        {
            date.month = 1;
            date.year++;
        }
    }

    return date;
}

unsigned int Date::diff(const Date & other)
{
    return day_count(*this) - day_count(other);
}

int Date::days_in_month() const
{
    return days_in_month(month, year);
}

Date Date::operator=(const Date & other)
{
    set_date(other.year, other.month, other.day);
    set_delim(other.delim);
    return *this;
}

bool Date::operator==(const Date & other) const
{
    return day == other.day && month == other.month && year == other.year;
}

bool Date::operator!=(const Date & other) const
{
    return !(*this == other);
}

bool Date::operator>(const Date &other) const
{
    return year > other.year || (year == other.year && month > other.month) || (year == other.year && month == other.month && day > other.day);
}

bool Date::operator>=(const Date &other) const
{
    return *this > other || *this == other;
}

bool Date::operator<(const Date &other) const
{
    return !(*this >= other);
}

bool Date::operator<=(const Date &other) const
{
    return !(*this > other);
}
