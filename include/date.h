#ifndef DATE_H
#define DATE_H

#include <string>

class Date
{
private:
    unsigned int day;
    unsigned int month;
    unsigned int year;

    char delim;

    bool validate_date(unsigned int, unsigned int, unsigned int) const;     // validates a given date
    unsigned int days_in_month(unsigned int, unsigned int) const;           // returns how many days in a given month and year
    int day_count(const Date &) const;                                      // returns the number of days since epoch (0000-00-00)
    unsigned int days_to_month(unsigned int, unsigned int) const;           // returns number of days to get to a month from beginning of year

public:
    Date()
        : day(0), month(0), year(0), delim('-')
    { return; }

    Date(const std::string &);                                              // sets date using format: yyyy-mm-dd
    Date(unsigned int, unsigned int, unsigned int, char = '-');             // sets the date
    Date(const Date &);

    bool set_date(const std::string &);                                     // sets date using format: yyyy-mm-dd
    bool set_date(unsigned int, unsigned int, unsigned int);                // sets the date
    bool set_delim(char);

    unsigned int get_day() const;
    unsigned int get_month() const;
    unsigned int get_year() const;
    std::string get_date() const;                                           // returns date in format: yyyy-mm-dd

    Date inc();                                                             // increments the date by one
    Date dec();                                                             // decrements the day by one

    Date date_plus_days(unsigned int);                                      // adds x days to the date
    unsigned int diff(const Date &);                                        // takes the difference in number of days between 2 dates

    int days_in_month() const;                                              // returns how many days are in the current month

    Date operator=(const Date &);                                           // assignment operator
    bool operator==(const Date &) const;                                    // equality operator
    bool operator!=(const Date &) const;                                    // comparison operators
    bool operator>(const Date &) const;
    bool operator>=(const Date &) const;
    bool operator<(const Date &) const;
    bool operator<=(const Date &) const;
};

#endif