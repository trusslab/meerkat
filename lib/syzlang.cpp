#include <iostream>
#include <string>
#include <vector>
#include <syzlang.h>

using namespace std;

// ======================================================================================================
// Wrapper

ParseClass TypeTag::get_class() const
{ return pc; }

string TypeTag::get_name() const
{ return name; }

int TypeTag::get_index() const
{ return index; }

// resourceClass, typeolClass, typemlClass, definitionClass, unionClass, structureClass, flagClass, syscallClass, includeClass
void TypeTag::set_type(const ParseClass &pclass, const string &n)
{
    pc = pclass;
    name = n;
    return;
}

// ======================================================================================================
// Base Class

string Identifier::get_name() const
{ return name; }

void Identifier::set_name(const string &n)
{
    name = n;
    return;
}

string Identifier::get_text() const
{ return text; }

void Identifier::set_text(const string &t)
{
    text = t;
    return;
}

// ======================================================================================================
// Member Classes

string TypeRef::print() const
{
    string p = name;

    if (this->has_opts())
    {
        p += "[";
        for (int i = 0; i < options.size(); i++)
            p += (i == 0 ? "" : ", ") + options.at(i).print();
        p += "]";
    }

    return p;
}

string TypeRef::get_name() const
{ return name;}

void TypeRef::set_name(const string &n)
{
    name = n;
    return;
}

bool TypeRef::has_opts() const
{ return !options.empty(); }

void TypeRef::push_opt(const TypeRef &tr)
{
    options.push_back(tr);
    return;
}

vector<TypeRef> TypeRef::get_opts() const
{ return options; }

void TypeRef::push_depends(vector<TypeTag> &needed, const vector<TypeTag> &items)
{
    int index;
    if(!checked_depends)
    {
        index = find_in_items(items, name);
        if (index >= 0)
            depends.push_back(items.at(index));

        for (TypeRef tr : options)
            tr.push_depends(depends, items);
        
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt.get_name()))
            needed.push_back(tt);

    return;
}

string TailingAttribute::print() const
{
    string p = name;

    if (has_n())
        p += "[" + to_string(n) + "]";

    return p;
}

string TailingAttribute::get_name() const
{ return name; }

void TailingAttribute::set_name(const string &n)
{
    name = n;
    return;
}

bool TailingAttribute::has_n() const
{ return n < 0 ? false : true; }

int TailingAttribute::get_n() const
{ return n; }

void TailingAttribute::set_n(int x)
{
    n = x;
    return;
}

string Field::print() const
{
    string p = name + " " + type.print();

    if (has_attrs())
    {
        p += " (";
        for (int i = 0; i < attributes.size(); i++)
            p += (i == 0 ? "" : ", ") + attributes.at(i).print();
        p += ")";
    }

    return p;
}

string Field::get_name() const
{ return name; }

void Field::set_name(const string &n)
{
    name = n;
    return;
}

TypeRef& Field::get_typeref()
{ return type; }

void Field::set_typeref(const TypeRef &tr)
{
    type = tr;
    return;
}

bool Field::has_attrs() const
{ return !attributes.empty(); }

void Field::push_attr(const TailingAttribute &ta)
{
    attributes.push_back(ta);
    return;
}

bool Field::check_attrs(const string &a) const
{
    for (TailingAttribute ta : attributes)
        if (ta.get_name() == a)
            return true;
    
    return false;
}

void Field::push_depends(vector<TypeTag> &needed, const vector<TypeTag> &items)
{
    int index;
    if (!checked_depends)
    {
        type.push_depends(depends, items);
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt.get_name()))
            needed.push_back(tt);

    return;
}

// ======================================================================================================
// Other directives

string Include::print() const
{
    return "include <" + name + ">";
}

string Definition::print() const
{
    return "define " + name;
}

void Definition::push_depends(vector<TypeTag> &needed, const vector<TypeTag> &items)
{ return; }

// ======================================================================================================
// Types

void BaseType::push_arg(const string &a)
{
    args.push_back(a);
    return;
}

vector<string>& BaseType::get_args()
{ return args; }

bool BaseType::has_args() const
{ return !args.empty(); }

string BaseType::print_args() const
{
    string p = "[";
    for (int i = 0; i < args.size(); i++)
        p += (i == 0 ? "" : ", ") + args.at(i);
    p += "]";

    return p;
}

string TypeOneline::print() const
{
    string p = "type " + name;

    if (has_args())
        p += " " + print_args();

    p += " " + type.print();
    return p;
}

TypeRef TypeOneline::get_type() const
{ return type; }

void TypeOneline::push_depends(vector<TypeTag> &needed, const vector<TypeTag> &items)
{
    if (!checked_depends)
    {
        type.push_depends(depends, items);
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt.get_name()))
            needed.push_back(tt);

    return;
}

string TypeMultiline::print() const
{
    string p = "type " + name;

    if (has_args())
        p += " " + print_args();

    p += " {\n";
    for (Field f : fields)
        p += "    " + f.print() + "\n";
    p += "}";

    return p;
}

vector<Field> TypeMultiline::get_fields() const
{ return fields; }

void TypeMultiline::push_depends(vector<TypeTag> &needed, const vector<TypeTag> &items)
{
    int index;
    if (!checked_depends)
    {
        for (Field f : fields)
            f.push_depends(depends, items);

        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt.get_name()))
            needed.push_back(tt);
    return;
}

// ======================================================================================================
// Resource

string Resource::print() const
{
    string p = "resource " + name + "[" + type.print() + "]";

    if (has_sv())
    {
        p += ": ";
        for (int i = 0; i < special_values.size(); i++)
            p += (i == 0 ? "" : ", ") + special_values.at(i);
    }

    return p;
}

TypeRef& Resource::get_typeref()
{ return type; }

bool Resource::has_sv() const
{ return !special_values.empty(); }

vector<string>& Resource::get_sv()
{ return special_values; }

void Resource::push_depends(vector<TypeTag> &needed, const vector<TypeTag> &items)
{ 
    int index;
    if (!checked_depends)
    {
        type.push_depends(depends, items);

        for (string v : special_values)
        {
            index = find_in_items(items, v);
            if (index >= 0)
                depends.push_back(items.at(index));
        }
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if(!is_in_needed(needed, tt.get_name()))
            needed.push_back(tt);

    return;
}

// ======================================================================================================
// Union/Structure

void BaseStruct::push_field(const Field &f)
{
    fields.push_back(f);
    return;
}

vector<Field> BaseStruct::get_fields() const
{ return fields; }

string BaseStruct::print_delim(char d = '{') const
{
    string p = name + " " + d + "\n";

    for (Field f : fields)
        p += "    " + f.print() + "\n";

    p += (d == '{' ? "}" : "]");

    return p;
}

void BaseStruct::push_depends(vector<TypeTag> &needed, const vector<TypeTag> &items)
{
    if (!checked_depends)
    {
        for (Field f : fields)
            f.push_depends(depends, items);
        
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt.get_name()))
            needed.push_back(tt);

    return;
}

string Union::print() const
{
    return print_delim('[');
}

string Structure::print() const
{
    return print_delim('{');
}

// ======================================================================================================
// Flag

string Flag::print() const
{
    string p = name + " = ";

    for (int i = 0; i < values.size(); i++)
        p += (i == 0 ? "" : ", ") + values.at(i);

    return p;
}

void Flag::push_depends(vector<TypeTag> &needed, const vector<TypeTag> &items)
{
    int index;
    if (!checked_depends)
    {
        for (string s : values)
        {
            index = find_in_items(items, s);
            if (index >= 0)
                depends.push_back(items.at(index));
        }
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt.get_name()))
            needed.push_back(tt);
    
    return;
}

// ======================================================================================================
// Syscall

string Syscall::print() const
{
    string p = name + "(";

    for (int i = 0; i < args.size(); i++)
        p += (i == 0 ? "" : ", ") + args.at(i).print();
    p += ")";

    if (has_return())
        p += " " + return_type;

    return p;
}

void Syscall::push_field(const Field &f)
{
    args.push_back(f);
    return;
}

void Syscall::set_return(const string &rt)
{
    return_type = rt;
    return;
}

bool Syscall::has_return() const
{ return !return_type.empty(); }

void Syscall::push_depends(vector<TypeTag> &needed, const vector<TypeTag> &items)
{
    int index;
    if (!checked_depends)
    {
        for (Field f : args)
            f.push_depends(depends, items);

        index = find_in_items(items, return_type);
        if (index >= 0)
            depends.push_back(items.at(index));
        
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt.get_name()))
            needed.push_back(tt);
    return;
}

vector<TypeTag> Syscall::get_resources_used(const vector<TypeTag> &items,
                const vector<TypeOneline> &typeols, const vector<TypeMultiline> &typemls,
                const vector<Union> &unions, const vector<Structure> &structures)
{
    int index;
    vector<TypeRef> items_to_check;
    if (!checked_used)
    {
        for (Field f : args)
        {
            items_to_check.clear();
            index = find_in_items(items, f.get_typeref().get_name());
            if (index >= 0 && items.at(index).get_class() == resourceClass)
                resources_used.push_back(items.at(index));
            else if (index >= 0)
            {
                // if it is an item, but not a resource.
                // Usually this means it is a typeol
                //cerr << "Warning: Non-resource item taken as input: " << f.get_typeref().get_name() << ".\n";
            }
            else if (f.get_typeref().get_name() == "ptr" && f.get_typeref().has_opts())
            {
                for (TypeRef tr : f.get_typeref().get_opts())
                    if (!is_in_typeref(items_to_check, tr.get_name()))
                        items_to_check.push_back(tr);
                
                if (items_to_check.size() > 0 && items_to_check.at(0).get_name() == "in")
                {
                    for (int i = 1; i < items_to_check.size(); i++)
                    {
                        index = find_in_items(items, items_to_check.at(i).get_name());
                        if (items_to_check.at(i).has_opts())
                        {
                            for (TypeRef tr : items_to_check.at(i).get_opts())
                                if (!is_in_typeref(items_to_check, tr.get_name()))
                                    items_to_check.push_back(tr);
                        }

                        if (index >= 0 && items.at(index).get_class() == resourceClass)
                        {
                            resources_used.push_back(items.at(index));
                        }
                        else if (index >= 0)
                        {
                            switch (items.at(index).get_class())
                            {
                            case structureClass:
                                for (Field ff : structures.at(items.at(index).get_index()).get_fields())
                                    if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            case unionClass:
                                for (Field ff : unions.at(items.at(index).get_index()).get_fields())
                                    if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            case typeolClass:
                                if (!is_in_typeref(items_to_check, items.at(index).get_name()))
                                    items_to_check.push_back(typeols.at(items.at(index).get_index()).get_type());
                                break;
                            case typemlClass:
                                for (Field ff : typemls.at(items.at(index).get_index()).get_fields())
                                    if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
                else if (items_to_check.at(0).get_name() == "inout")
                {
                    for (int i = 1; i < items_to_check.size(); i++)
                    {
                        index = find_in_items(items, items_to_check.at(i).get_name());
                        if (index >= 0 && items.at(index).get_class() == resourceClass)
                        {
                            resources_used.push_back(items.at(index));
                        }
                        else if (index >= 0 && items.at(index).get_class() == structureClass)
                        {
                            for (Field ff : structures.at(items.at(index).get_index()).get_fields())
                                if (ff.has_attrs() && (ff.check_attrs("in") || ff.check_attrs("inout")))
                                    items_to_check.push_back(ff.get_typeref());
                        }
                    }
                }
            }
        }
        checked_used = true;
    }
    return resources_used;
}

vector<TypeTag> Syscall::get_resources_produced(const vector<TypeTag> &items,
                    const vector<TypeOneline> &typeols, const vector<TypeMultiline> &typemls,
                    const vector<Union> &unions, const vector<Structure> &structures)
{
    int index;
    vector<TypeRef> items_to_check;
    if (!checked_produced)
    {
        for (Field f : args)
        {
            items_to_check.clear();
            if (f.get_typeref().get_name() == "ptr" && f.get_typeref().has_opts())
            {
                for (TypeRef tr : f.get_typeref().get_opts())
                    if (!is_in_typeref(items_to_check, tr.get_name()))
                        items_to_check.push_back(tr);
                
                if (items_to_check.size() > 0 && items_to_check.at(0).get_name() == "out")
                {
                    for (int i = 1; i < items_to_check.size(); i++)
                    {
                        index = find_in_items(items, items_to_check.at(i).get_name());
                        if (items_to_check.at(i).has_opts())
                        {
                            for (TypeRef tr : items_to_check.at(i).get_opts())
                                if (!is_in_typeref(items_to_check, tr.get_name()))
                                    items_to_check.push_back(tr);
                        }

                        if (index >= 0 && items.at(index).get_class() == resourceClass)
                        {
                            resources_produced.push_back(items.at(index));
                        }
                        else if (index >= 0)
                        {
                            switch (items.at(index).get_class())
                            {
                            case structureClass:
                                for (Field ff : structures.at(items.at(index).get_index()).get_fields())
                                    if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            case unionClass:
                                for (Field ff : unions.at(items.at(index).get_index()).get_fields())
                                    if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            case typeolClass:
                                if (!is_in_typeref(items_to_check, items.at(index).get_name()))
                                    items_to_check.push_back(typeols.at(items.at(index).get_index()).get_type());
                                break;
                            case typemlClass:
                                for (Field ff : typemls.at(items.at(index).get_index()).get_fields())
                                    if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
                else if (items_to_check.at(0).get_name() == "inout")
                {
                    for (int i = 1; i < items_to_check.size(); i++)
                    {
                        index = find_in_items(items, items_to_check.at(i).get_name());
                        if (index >= 0 && items.at(index).get_class() == resourceClass)
                        {
                            resources_produced.push_back(items.at(index));
                        }
                        else if (index >= 0 && items.at(index).get_class() == structureClass)
                        {
                            for (Field ff : structures.at(items.at(index).get_index()).get_fields())
                                if (ff.has_attrs() && (ff.check_attrs("out") || ff.check_attrs("inout")))
                                    items_to_check.push_back(ff.get_typeref());
                        }
                    }
                }
            }
        }

        if (has_return())
        {
            index = find_in_items(items, return_type);
            if (index >= 0)
                resources_produced.push_back(items.at(index));
            else
                cerr << "Warning: Bad return value " << return_type << ".\n";
        }
        checked_produced = true;
    }
    return resources_produced;
}

int Syscall::total_resources(const vector<TypeTag> &items, const vector<TypeOneline> &typeols,
                                const vector<TypeMultiline> &typemls, const vector<Union> &unions,
                                const vector<Structure> &structures)
{
    if (!checked_produced || !checked_used)
    {
        get_resources_used(items, typeols, typemls, unions, structures);
        get_resources_produced(items, typeols, typemls, unions, structures);
    }

    return resources_used.size() + resources_produced.size();
}

// ======================================================================================================
// Useful funcs

// insert tt into items sorted using binary search
// l = 0, smaller values
// h = high index, larger values
void item_push_sorted(vector<TypeTag> &items, const TypeTag &tt)
{
    if (items.empty())
    {
        items.push_back(tt);
        return;
    }

    int l = 0, h = items.size() - 1, m;
    while (l <= h)
    {
        m = (l + h) / 2;
        if (tt.get_name() > items.at(m).get_name())
            l = m + 1;
        else
            h = m - 1;
    }

    items.insert(items.begin() + l, tt);
    return;
}

int find_in_items(const vector<TypeTag> &items, const string &name)
{
    if (items.empty())
        return -1;

    int l = 0, h = items.size() - 1, m;
    while (l <= h)
    {
        m = (l + h) / 2;
        if (name == items.at(m).get_name())
            return m;
        else if (name > items.at(m).get_name())
            l = m + 1;
        else
            h = m - 1;
    }

    if (l >= items.size() || h < 0)
        return -1;

    return (items.at(l).get_name() == name ? l : -1);
}

// binary search on the items
bool is_in_items(const vector<TypeTag> &items, const string &name)
{
    return (find_in_items(items, name) >= 0);
}

// needed cannot be sorted because of how we add things to it.
bool is_in_needed(const vector<TypeTag> &needed, const string &name)
{
    for (TypeTag tt : needed)
        if (name == tt.get_name())
            return true;

    return false;
}

// linear search because sorting is a lot of effort for not a lot of time gained
bool is_in_includes(const vector<Include> &includes, const string &name)
{
    for (Include i : includes)
        if (i.get_name() == name)
            return true;
    return false;
}

bool is_in_typeref(const vector<TypeRef> &typerefs, const string &name)
{
    for (TypeRef tr : typerefs)
        if (tr.get_name() == name)
            return true;
    
    return false;
}

int find_in_syscalls(const vector<Syscall> &syscalls, const string &name)
{
    int i = 0;
    for (i = 0; i < syscalls.size(); i++)
        if (syscalls.at(i).get_name() == name)
            return i;
    
    return -1;
}

int stoi_custom(const string &line)
{
    int n = 0, neg = 1;
    if (line.empty())
        return -1;
    
    neg = (line.at(0) == '-' ? -1 : neg);

    for (int i = (neg < 0 ? 1 : 0); i < line.size(); i++)
        n = (n * 10) + (line.at(i) - '0');
    
    return n * neg;
}
