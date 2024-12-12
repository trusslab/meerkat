#include <json.h>

#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <stdexcept>

std::string json_type(JSON_Value_Type vtype)
{
    switch (vtype)
    {
    case JSON_Val_bool:
        return "JSON_Val_bool";
    case JSON_Val_int:
        return "JSON_Val_int";
    case JSON_Val_double:
        return "JSON_Val_double";
    case JSON_Val_string:
        return "JSON_Val_string";
    case JSON_Val_object:
        return "JSON_Val_object";
    case JSON_Val_list:
        return "JSON_Val_list";
    default:
        break;
    }
    return "";
}

void throw_invalid_name(const std::string &name)
{
    throw std::invalid_argument(name);
}

void throw_invalid_type(JSON_Value_Type vtype)
{
    throw std::invalid_argument(json_type(vtype));
}

void json_check_type(JSON_Value_Type actual, JSON_Value_Type vtype)
{
    if (vtype != actual)
        throw_invalid_type(vtype);
    return;
}

// JSON_Value

JSON_Value &JSON_Value::operator=(const JSON_Value &other)
{
    vtype = other.vtype;
    switch (vtype)
    {
    case JSON_Val_bool:
        bvalue = other.bvalue;
        break;
    case JSON_Val_int:
        ivalue = other.ivalue;
        break;
    case JSON_Val_double:
        dvalue = other.dvalue;
        break;
    case JSON_Val_string:
        svalue = other.svalue;
        break;
    case JSON_Val_object:
        object = other.object;
        break;
    case JSON_Val_list:
        list = other.list;
        break;
    default:
        vtype = JSON_Val_uninit;
    }
    return *this;
}

JSON_Value &JSON_Value::operator=(bool b)
{
    vtype = JSON_Val_bool;
    bvalue = b;
    return *this;
}

JSON_Value &JSON_Value::operator=(int i)
{
    vtype = JSON_Val_int;
    ivalue = i;
    return *this;
}

JSON_Value &JSON_Value::operator=(double d)
{
    vtype = JSON_Val_double;
    dvalue = d;
    return *this;
}

JSON_Value &JSON_Value::operator=(const std::string &s)
{
    vtype = JSON_Val_string;
    svalue = s;
    return *this;
}

JSON_Value &JSON_Value::operator=(const JSON_Object &o)
{
    vtype = JSON_Val_object;
    object = o;
    return *this;
}

JSON_Value &JSON_Value::operator=(const JSON_List &l)
{
    vtype = JSON_Val_list;
    list = l;
    return *this;
}

JSON_Value_Type JSON_Value::type() const
{ return vtype; }

bool JSON_Value::is_type(JSON_Value_Type t) const
{ return t == vtype; }

const bool& JSON_Value::get_bool() const
{
    json_check_type(JSON_Val_bool, vtype);
    return bvalue;
}

const int& JSON_Value::get_int() const
{
    json_check_type(JSON_Val_int, vtype);
    return ivalue;
}

const double& JSON_Value::get_double() const
{
    json_check_type(JSON_Val_double, vtype);
    return dvalue;
}

const std::string& JSON_Value::get_string() const
{
    json_check_type(JSON_Val_string, vtype);
    return svalue;
}

const JSON_Object& JSON_Value::get_object() const
{
    json_check_type(JSON_Val_object, vtype);
    return object;
}

const JSON_List& JSON_Value::get_list() const
{
    json_check_type(JSON_Val_list, vtype);
    return list;
}

void JSON_Value::clear()
{
    if (vtype == JSON_Val_bool)
    {
        bvalue = false;
    }
    else if (vtype == JSON_Val_int)
    {
        ivalue = 0;
    }
    else if (vtype == JSON_Val_double)
    {
        dvalue = 0;
    }
    else if (vtype == JSON_Val_string)
    {
        svalue.clear();
    }
    else if (vtype == JSON_Val_object)
    {
        object.clear();
    }
    else if (vtype == JSON_Val_list)
    {
        list.clear();
    }
    vtype = JSON_Val_uninit;
}

// JSON

JSON &JSON::operator=(const JSON_Object &obj)
{
    object.clear();
    object = obj;
    return *this;
}

std::istream& json_getline(std::ifstream &inf, std::string &line, unsigned int &index)
{
    index = 0;
    line.clear();
    return getline(inf, line);
}

bool json_get_next(std::ifstream &inf, std::string &line, unsigned int &index)
{
    // TODO: work with tabs
    do {
        for (; index < line.size() && isspace(line.at(index)); index++);
        if (index < line.size())
            break;
    } while (json_getline(inf, line, index));
    return inf.peek() != EOF;
}

bool json_parse_list(std::ifstream &, std::string &, unsigned int &, JSON_List &);
bool json_parse_object(std::ifstream &, std::string &, unsigned int &, JSON_Object &);

bool json_parse_bool(std::string &line, unsigned int &index, bool &val)
{
    std::string str;
    unsigned int pos = index + 1;
    for (; pos < line.size() && isalpha(line.at(pos)); pos++);
    if (pos < line.size())
        str = line.substr(index, pos - index);
    else
        str = line.substr(index);
    
    for (char &c : str)
        c = tolower(c);

    index = pos;
    if (str == "true")
        val = true;
    else if (str == "false")
        val = false;
    else
        return false;
    return true;
}

// parses an int starting at the first digit
// returned index is just after the parsed int
// returns false on error, true on success
bool json_parse_int(std::string &line, unsigned int &index, int &val)
{
    std::string str;
    unsigned int pos = index + 1;
    if (pos < line.size() && line.at(pos) == '-')
        pos++;
    for (; pos < line.size() && isdigit(line.at(pos)); pos++);
    if (pos < line.size())
        str = line.substr(index, pos - index);
    else
        str = line.substr(index);

    val = std::stoi(str);
    index = pos;
    return true;
}

// parses a double starting at the first digit
// returned index is just after the parsed double
// returns false on error, true on success
bool json_parse_double(std::string &line, unsigned int &index, double &val)
{
    std::string str;
    unsigned int pos = index + 1;
    if (pos < line.size() && line.at(pos) == '-')
        pos++;
    for (; pos < line.size() && (isdigit(line.at(pos)) || line.at(pos) == '.'); pos++);
    if (pos < line.size())
        str = line.substr(index, pos - index);
    else
        str = line.substr(index);

    val = std::stod(str);
    index = pos;
    return true;
}

// parses a string starting at the '\"'
// returned index is just after the parsed string
// returns false on error, true on success
bool json_parse_string(std::string &line, unsigned int &index, std::string &str)
{
    unsigned int pos;
    // quotes should not break lines
    pos = line.find_first_of("\"", index + 1);
    if (pos == std::string::npos)
        return false;
    str = line.substr(index + 1, pos - index - 1);
    // validate name?
    index = pos + 1;
    return true;
}

JSON_Value_Type json_classify_value(std::string &line, unsigned int &index)
{
    if (index >= line.size())
        return JSON_Val_uninit;
    
    if (line.at(index) == '[')
        return JSON_Val_list;
    else if (line.at(index) == '{')
        return JSON_Val_object;
    else if (line.at(index) == '\"')
        return JSON_Val_string;
    else if (tolower(line.at(index)) == 't' || tolower(line.at(index)) == 'f')
        return JSON_Val_bool;
    else if (isdigit(line.at(index)) || line.at(index) == '-')
    {
        unsigned int i = index + 1;
        if (i < line.size() && line.at(i) == '-')
            i++;
        for (; i < line.size() && isdigit(line.at(i)); i++);
        if (i < line.size() && line.at(i) == '.')
            return JSON_Val_double;
        return JSON_Val_int;
    }
    return JSON_Val_uninit;
}

bool json_parse_value(std::ifstream &inf, std::string &line, unsigned int &index, JSON_Value &value)
{
    JSON_Value_Type vtype = json_classify_value(line, index);
    if (vtype == JSON_Val_bool)
    {
        bool bval;
        if (!json_parse_bool(line, index, bval))
            return false;
        value = bval;
        return true;
    }
    else if (vtype == JSON_Val_int)
    {
        int ival;
        if (!json_parse_int(line, index, ival))
            return false;
        value = ival;
        return true;
    }
    else if (vtype == JSON_Val_double)
    {
        double dval;
        if (!json_parse_double(line, index, dval))
            return false;
        value = dval;
        return true;
    }
    else if (vtype == JSON_Val_string)
    {
        std::string sval;
        if (!json_parse_string(line, index, sval))
            return false;
        value = sval;
        return true;
    }
    else if (vtype == JSON_Val_object)
    {
        JSON_Object oval;
        if (!json_parse_object(inf, line, index, oval))
            return false;
        value = oval;
        return true;
    }
    else if (vtype == JSON_Val_list)
    {
        JSON_List lval;
        if (!json_parse_list(inf, line, index, lval))
            return false;
        value = lval;
        return true;
    }
    else
    { return false; }

    return true;
}

// parse json file for a name-value pair starting at the '\"'
// returns true on success, false on failure
bool json_parse_data(std::ifstream &inf, std::string &line, unsigned int &index, JSON_Object &object)
{
    bool err = false;
    std::string name;
    JSON_Value value;

    if (index >= line.size() || line.at(index) != '\"')
        return false;
    
    if (!json_parse_string(line, index, name))
        return false;
    
    if (!json_get_next(inf, line, index))
        return false;

    if (line.at(index) != ':')
        return false;

    if (!json_get_next(inf, line, ++index))
        return false;

    if (!json_parse_value(inf, line, index, value))
        return false;

    object.insert({name, value});
    return true;
}

// parse json file for a list starting at the '['
// returned index is just after the parsed list
// returns true on success, false on failure
bool json_parse_list(std::ifstream &inf, std::string &line, unsigned int &index, JSON_List &list)
{
    JSON_Value value;
    if (index >= line.size() || line.at(index) != '[')
        return false;
    while (json_get_next(inf, line, ++index))
    {
        value.clear();
        if (!json_parse_value(inf, line, index, value))
            return false;
        
        list.push_back(value);

        // find next ',' or ']'
        if (!json_get_next(inf, line, index))
            return false;
        
        switch (line.at(index))
        {
        case ',':
            continue;
        case ']':
            index++;
            return true;
        default:
            return false;
        }
    };
    return false;
}

// parse json file for a specific object starting at the '{'
// returned index is just after the parsed object
// returns true on success, false on failure
bool json_parse_object(std::ifstream &inf, std::string &line, unsigned int &index, JSON_Object &object)
{
    if (index >= line.size() || line.at(index) != '{')
        return false;
    while (json_get_next(inf, line, ++index))
    {
        if (!json_parse_data(inf, line, index, object))
            return false;
        
        // find next ',' or '}'
        json_get_next(inf, line, index);
        if (index >= line.size())
            return false;
        
        switch (line.at(index))
        {
        case ',':
            continue;
        case '}':
            index++;
            return true;
        default:
            return false;
        }
    }
    return false;
}

// parse a json file and store in a json object.
// returns true on success, false on failure.
bool JSON::parse(const std::string &filename)
{
    bool err = true;
    unsigned int index = 0;
    std::string line;
    std::ifstream inf;
    inf.open(filename);
    if (!inf)
        return false;

    line.clear();
    err = json_get_next(inf, line, index);
    if (!err)
        goto error;
    err = json_parse_object(inf, line, index, object);
    if (err) // clean exit
        goto exit;
error:
    object.clear();
exit:
    inf.close();
    return err;
}

void JSON::clear()
{ object.clear(); }

bool JSON::has_name(const std::string &name) const
{ return object.count(name) > 0; }

JSON_Value_Type JSON::type(const std::string &name) const
{
    if (!has_name(name))
        throw_invalid_name(name);
    
    return object.at(name).type();
}

bool JSON::is_type(const std::string &name, JSON_Value_Type t) const
{
    if (!has_name(name))
        throw_invalid_name(name);
    
    return object.at(name).type() == t;
}

const bool& JSON::get_bool(const std::string &name) const
{
    if (!has_name(name))
        throw_invalid_name(name);
    
    return object.at(name).get_bool();
}

const int& JSON::get_int(const std::string &name) const
{
    if (!has_name(name))
        throw_invalid_name(name);
    
    return object.at(name).get_int();
}

const double& JSON::get_double(const std::string &name) const
{
    if (!has_name(name))
        throw_invalid_name(name);
    
    return object.at(name).get_double();
}

const std::string& JSON::get_string(const std::string &name) const
{
    if (!has_name(name))
        throw_invalid_name(name);
    
    return object.at(name).get_string();
}

const JSON_Object& JSON::get_object(const std::string &name) const
{
    if (!has_name(name))
        throw_invalid_name(name);
    
    return object.at(name).get_object();
}

const JSON_List& JSON::get_list(const std::string &name) const
{
    if (!has_name(name))
        throw_invalid_name(name);
    
    return object.at(name).get_list();
}

const JSON JSON::get_json(const std::string &name) const
{
    if (!has_name(name))
        throw_invalid_name(name);

    return JSON(object.at(name).get_object());
}
