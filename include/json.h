#ifndef JSON_H
#define JSON_H

#include <string>
#include <vector>
#include <map>

enum JSON_Value_Type {JSON_Val_bool, JSON_Val_int, JSON_Val_double, JSON_Val_string, JSON_Val_object, JSON_Val_list, JSON_Val_uninit};

class JSON_Value;

typedef std::map<std::string, JSON_Value> JSON_Object;
typedef std::vector<JSON_Value> JSON_List;

class JSON_Value
{
protected:
    bool bvalue;
    int ivalue;
    double dvalue;
    std::string svalue;
    JSON_Object object;
    JSON_List list;

    JSON_Value_Type vtype;

public:
    JSON_Value()
    { vtype = JSON_Val_uninit; }
    JSON_Value(const bool b)
    { vtype = JSON_Val_bool; bvalue = b; }
    JSON_Value(const int i)
    { vtype = JSON_Val_int; ivalue = i; }
    JSON_Value(const double d)
    { vtype = JSON_Val_double; dvalue = d; }
    JSON_Value(const std::string & s)
    { vtype = JSON_Val_string; svalue = s; }
    JSON_Value(const JSON_Object & o)
    { vtype = JSON_Val_object; object = o; }
    JSON_Value(const JSON_List & l)
    { vtype = JSON_Val_list; list = l; }
    JSON_Value(const JSON_Value &other)
    { *this = other; }

    JSON_Value &operator=(const JSON_Value &);
    JSON_Value &operator=(bool);
    JSON_Value &operator=(int);
    JSON_Value &operator=(double);
    JSON_Value &operator=(const std::string &);
    JSON_Value &operator=(const JSON_Object &);
    JSON_Value &operator=(const JSON_List &);

    JSON_Value_Type type() const;
    bool is_type(JSON_Value_Type) const;

    const bool& get_bool() const;
    const int& get_int() const;
    const double& get_double() const;
    const std::string& get_string() const;
    const JSON_Object& get_object() const;
    const JSON_List& get_list() const;

    void clear();
};

class JSON
{
private:
    JSON_Object object;

public:
    JSON()
    { return; }

    JSON(const JSON_Object &obj)
        : object(obj)
    { return; }

    JSON &operator=(const JSON_Object &);

    bool parse(const std::string &);
    void clear();

    bool has_name(const std::string &) const;

    JSON_Value_Type type(const std::string &) const;
    bool is_type(const std::string &, JSON_Value_Type) const;

    const bool& get_bool(const std::string &) const;
    const int& get_int(const std::string &) const;
    const double& get_double(const std::string &) const;
    const std::string& get_string(const std::string &) const;
    const JSON_Object& get_object(const std::string &) const;
    const JSON_List& get_list(const std::string &) const;

    const JSON get_json(const std::string &) const;
};

#endif
