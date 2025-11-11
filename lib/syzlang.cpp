#include <iostream>
#include <string>
#include <vector>
#include <syzlang.h>

// ======================================================================================================
// Wrapper

ParseClass TypeTag::get_class() const
{ return pc; }

std::string TypeTag::get_name() const
{ return name; }

int TypeTag::get_index() const
{ return index; }

// resourceClass, typeolClass, typemlClass, definitionClass, unionClass, structureClass, flagClass, syscallClass, includeClass
void TypeTag::set_type(const ParseClass &pclass, const std::string &n)
{
    pc = pclass;
    name = n;
    return;
}

// ======================================================================================================
// Base Class

std::string Identifier::get_name() const
{ return name; }

void Identifier::set_name(const std::string &n)
{
    name = n;
    return;
}

std::string Identifier::get_text() const
{ return text; }

void Identifier::set_text(const std::string &t)
{
    text = t;
    return;
}

// ======================================================================================================
// Member Classes

std::string TypeRef::print() const
{
    std::string p = name;

    if (this->has_opts())
    {
        p += "[";
        for (int i = 0; i < options.size(); i++)
            p += (i == 0 ? "" : ", ") + options.at(i).print();
        p += "]";
    }

    return p;
}

std::string TypeRef::get_name() const
{ return name;}

void TypeRef::set_name(const std::string &n)
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

std::vector<TypeRef> TypeRef::get_opts() const
{ return options; }

void TypeRef::push_depends(std::vector<TypeTag> &needed, const std::vector<TypeTag> &items)
{
    int index;
    if(!checked_depends)
    {
        for (TypeTag tt : find_in_items(items, name))
            depends.push_back(tt);

        for (TypeRef tr : options)
            tr.push_depends(depends, items);
        
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt))
            needed.push_back(tt);

    return;
}

std::string TailingAttribute::print() const
{
    std::string p = name;

    if (has_n())
        p += "[" + std::to_string(n) + "]";

    return p;
}

std::string TailingAttribute::get_name() const
{ return name; }

void TailingAttribute::set_name(const std::string &n)
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

std::string Field::print() const
{
    std::string p = name + " " + type.print();

    if (has_attrs())
    {
        p += " (";
        for (int i = 0; i < attributes.size(); i++)
            p += (i == 0 ? "" : ", ") + attributes.at(i).print();
        p += ")";
    }

    return p;
}

std::string Field::get_name() const
{ return name; }

void Field::set_name(const std::string &n)
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

bool Field::check_attrs(const std::string &a) const
{
    for (TailingAttribute ta : attributes)
        if (ta.get_name() == a)
            return true;
    
    return false;
}

void Field::push_depends(std::vector<TypeTag> &needed, const std::vector<TypeTag> &items)
{
    int index;
    if (!checked_depends)
    {
        type.push_depends(depends, items);
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt))
            needed.push_back(tt);

    return;
}

// ======================================================================================================
// Other directives

std::string Include::print() const
{
    return "include <" + name + ">";
}

std::string Definition::print() const
{
    return "define " + name;
}

void Definition::push_depends(std::vector<TypeTag> &needed, const std::vector<TypeTag> &items)
{ return; }

// ======================================================================================================
// Types

void BaseType::push_arg(const std::string &a)
{
    args.push_back(a);
    return;
}

std::vector<std::string>& BaseType::get_args()
{ return args; }

bool BaseType::has_args() const
{ return !args.empty(); }

int BaseType::find_arg(const std::string &query) const
{
    for (int i = 0; i < args.size(); i++)
        if (args.at(i) == query)
            return i;

    return -1;
}

std::string BaseType::print_args() const
{
    std::string p = "[";
    for (int i = 0; i < args.size(); i++)
        p += (i == 0 ? "" : ", ") + args.at(i);
    p += "]";

    return p;
}

std::string TypeOneline::print() const
{
    std::string p = "type " + name;

    if (has_args())
        p += " " + print_args();

    p += " " + type.print();
    return p;
}

TypeRef TypeOneline::get_type() const
{ return type; }

void TypeOneline::push_depends(std::vector<TypeTag> &needed, const std::vector<TypeTag> &items)
{
    if (!checked_depends)
    {
        type.push_depends(depends, items);
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt))
            needed.push_back(tt);

    return;
}

std::string TypeMultiline::print() const
{
    std::string p = "type " + name;

    if (has_args())
        p += " " + print_args();

    p += " {\n";
    for (Field f : fields)
        p += "    " + f.print() + "\n";
    p += "}";

    return p;
}

std::vector<Field> TypeMultiline::get_fields() const
{ return fields; }

void TypeMultiline::push_depends(std::vector<TypeTag> &needed, const std::vector<TypeTag> &items)
{
    int index;
    if (!checked_depends)
    {
        for (Field f : fields)
            f.push_depends(depends, items);

        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt))
            needed.push_back(tt);
    return;
}

// ======================================================================================================
// Resource

std::string Resource::print() const
{
    std::string p = "resource " + name + "[" + type.print() + "]";

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

std::vector<std::string>& Resource::get_sv()
{ return special_values; }

void Resource::push_depends(std::vector<TypeTag> &needed, const std::vector<TypeTag> &items)
{ 
    int index;
    if (!checked_depends)
    {
        type.push_depends(depends, items);

        for (std::string v : special_values)
        {
            index = find_in_items(items, TypeTag(definitionClass, v));
            if (index >= 0)
                depends.push_back(items.at(index));
        }
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if(!is_in_needed(needed, tt))
            needed.push_back(tt);

    return;
}

bool Resource::has_depends()
{ // This can be part of identifier if needed.
    if (!checked_depends)
    {
        std::cerr << "Error: Tried to query depends before they were checked\n" << std::flush;
        return 0;
    }

    return depends.size() > 0;
}

// ======================================================================================================
// Union/Structure

void BaseStruct::push_field(const Field &f)
{
    fields.push_back(f);
    return;
}

std::vector<Field> BaseStruct::get_fields() const
{ return fields; }

std::string BaseStruct::print_delim(char d = '{') const
{
    std::string p = name + " " + d + "\n";

    for (Field f : fields)
        p += "    " + f.print() + "\n";

    p += (d == '{' ? "}" : "]");

    return p;
}

void BaseStruct::push_depends(std::vector<TypeTag> &needed, const std::vector<TypeTag> &items)
{
    if (!checked_depends)
    {
        for (Field f : fields)
            f.push_depends(depends, items);
        
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt))
            needed.push_back(tt);

    return;
}

std::string Union::print() const
{
    return print_delim('[');
}

std::string Structure::print() const
{
    return print_delim('{');
}

int Structure::find_outoverlay() const
{
    for (int i = 0; i < fields.size(); i++)
        if (fields.at(i).check_attrs("out_overlay"))
            return i;

    return -1;
}

// ======================================================================================================
// Flag

std::string Flag::print() const
{
    std::string p = name + " = ";

    for (int i = 0; i < values.size(); i++)
        p += (i == 0 ? "" : ", ") + values.at(i);

    return p;
}

void Flag::push_depends(std::vector<TypeTag> &needed, const std::vector<TypeTag> &items)
{
    int index;
    if (!checked_depends)
    {
        for (std::string s : values)
        {
            index = find_in_items(items, TypeTag(definitionClass, s));
            if (index >= 0)
                depends.push_back(items.at(index));
        }
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt))
            needed.push_back(tt);
    
    return;
}

// ======================================================================================================
// Syscall

std::string Syscall::print() const
{
    std::string p = name + "(";

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

void Syscall::set_return(const std::string &rt)
{
    return_type = rt;
    return;
}

bool Syscall::has_return() const
{ return !return_type.empty(); }

void Syscall::push_depends(std::vector<TypeTag> &needed, const std::vector<TypeTag> &items)
{
    int index;
    if (!checked_depends)
    {
        for (Field f : args)
            f.push_depends(depends, items);

        index = find_in_items(items, TypeTag(resourceClass, return_type));
        if (index >= 0)
            depends.push_back(items.at(index));
        
        checked_depends = true;
    }

    for (TypeTag tt : depends)
        if (!is_in_needed(needed, tt))
            needed.push_back(tt);
    return;
}

std::vector<TypeTag> Syscall::get_resources_used(const std::vector<TypeTag> &items,
                const std::vector<TypeOneline> &typeols, const std::vector<TypeMultiline> &typemls,
                const std::vector<Union> &unions, const std::vector<Structure> &structures)
{
    int index;
    std::vector<TypeRef> items_to_check;
    TypeRef tmp;
    if (!checked_used)
    {
        for (Field f : args)
        {
            // For each field in the syscall
            items_to_check.clear();
            // if the field is directly a resource, keep it
            index = find_in_items(items, TypeTag(resourceClass, f.get_typeref().get_name()));
            if (index >= 0 && items.at(index).get_class() == resourceClass)
                resources_used.push_back(items.at(index));
            // if the field is a pointer, look into it (pointers are how other complex classes are referenced)
            else if (f.get_typeref().get_name() == "ptr" && f.get_typeref().has_opts())
            {
                // always check the options passed to complex types
                for (TypeRef tr : f.get_typeref().get_opts())
                    if (!is_in_typeref(items_to_check, tr.get_name()))
                        items_to_check.push_back(tr);
                
                // look for pointers with the "in" direction
                if (items_to_check.size() > 0 && items_to_check.at(0).get_name() == "in")
                {
                    // for each other item to check
                    for (int i = 1; i < items_to_check.size(); i++)
                    {
                        // add all of its options if there are any
                        for (TypeRef tr : items_to_check.at(i).get_opts())
                            if (!is_in_typeref(items_to_check, tr.get_name()))
                                items_to_check.push_back(tr);

                        // for each item with the same name...
                        // (we ignore that it is only referring to one since items that
                        // share a name always go together, and usually it's just types and flags)
                        for (TypeTag tt : find_in_items(items, items_to_check.at(i).get_name()))
                        {
                            // switch on the class. Ether keep the resource, or go deeper
                            switch (tt.get_class())
                            {
                            case resourceClass:
                                resources_used.push_back(tt);
                                break;
                            case structureClass:
                                for (Field ff : structures.at(tt.get_index()).get_fields())
                                    if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()) && (!ff.get_typeref().has_opts() || ff.get_typeref().get_opts().at(0).get_name() != "out"))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            case unionClass:
                                for (Field ff : unions.at(tt.get_index()).get_fields())
                                    if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            case typeolClass:
                                if (!is_in_typeref(items_to_check, typeols.at(tt.get_index()).get_type().get_name()))
                                    items_to_check.push_back(typeols.at(tt.get_index()).get_type());
                                break;
                            case typemlClass:
                                for (Field ff : typemls.at(tt.get_index()).get_fields())
                                    if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
                // pointers with "inout" are a little more complicated
                else if (items_to_check.at(0).get_name() == "inout")
                {
                    // once again for all the items to check...
                    for (int i = 1; i < items_to_check.size(); i++)
                    {
                        for (TypeTag tt : find_in_items(items, items_to_check.at(i).get_name()))
                        {
                            // switch on the class of the item
                            switch (tt.get_class())
                            {
                            case resourceClass:
                                resources_used.push_back(tt);
                                break;
                            case structureClass:
                                // Check for out_overlay
                                index = structures.at(tt.get_index()).find_outoverlay();
                                if (index >= 0)
                                {
                                    for (int i = 0; i < structures.at(tt.get_index()).get_fields().size() && i < index; i++)
                                        if (!is_in_typeref(items_to_check, structures.at(tt.get_index()).get_fields().at(i).get_typeref().get_name()))
                                            items_to_check.push_back(structures.at(tt.get_index()).get_fields().at(i).get_typeref());
                                    break;
                                }

                                for (Field ff : structures.at(tt.get_index()).get_fields())
                                    if ((ff.has_attrs() && (ff.check_attrs("in") || ff.check_attrs("inout"))))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            case typeolClass:
                                if (!is_in_typeref(items_to_check, typeols.at(tt.get_index()).get_type().get_name()))
                                    items_to_check.push_back(typeols.at(tt.get_index()).get_type());
                                break;
                            case typemlClass:
                                for (Field ff : typemls.at(tt.get_index()).get_fields())
                                {
                                    if (ff.has_attrs() && (ff.check_attrs("in") || ff.check_attrs("inout")))
                                    {
                                        // Keep track of passed args in type templates
                                        // re-create the typeref with the proper name so we can check it later
                                        index = typemls.at(tt.get_index()).find_arg(ff.get_typeref().get_name());
                                        if (index >= 0)
                                            tmp = TypeRef(items_to_check.at(i).get_opts().at(index).get_name(), ff.get_typeref().get_opts());
                                        else
                                            tmp = ff.get_typeref();
                                        
                                        if (!is_in_typeref(items_to_check, tmp.get_name()))
                                            items_to_check.push_back(tmp);
                                    }
                                }
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
            }
        }
        checked_used = true;
    }
    return resources_used;
}

std::vector<TypeTag> Syscall::get_resources_produced(const std::vector<TypeTag> &items,
                    const std::vector<TypeOneline> &typeols, const std::vector<TypeMultiline> &typemls,
                    const std::vector<Union> &unions, const std::vector<Structure> &structures)
{
    int index;
    std::vector<TypeRef> items_to_check;
    TypeRef tmp;
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
                        for (TypeRef tr : items_to_check.at(i).get_opts())
                            if (!is_in_typeref(items_to_check, tr.get_name()))
                                items_to_check.push_back(tr);
                        
                        for (TypeTag tt : find_in_items(items, items_to_check.at(i).get_name()))
                        {
                            if (tt.get_class() == resourceClass)
                            {
                                resources_produced.push_back(tt);
                            }
                            else
                            {
                                switch (tt.get_class())
                                {
                                case structureClass:
                                    for (Field ff : structures.at(tt.get_index()).get_fields())
                                        if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()) && (!ff.get_typeref().has_opts() || ff.get_typeref().get_opts().at(0).get_name() != "in"))
                                            items_to_check.push_back(ff.get_typeref());
                                    break;
                                case unionClass:
                                    for (Field ff : unions.at(tt.get_index()).get_fields())
                                        if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                            items_to_check.push_back(ff.get_typeref());
                                    break;
                                case typeolClass:
                                    if (!is_in_typeref(items_to_check, typeols.at(tt.get_index()).get_type().get_name()))
                                        items_to_check.push_back(typeols.at(tt.get_index()).get_type());
                                    break;
                                case typemlClass:
                                    for (Field ff : typemls.at(tt.get_index()).get_fields())
                                        if (!is_in_typeref(items_to_check, ff.get_typeref().get_name()))
                                            items_to_check.push_back(ff.get_typeref());
                                    break;
                                default:
                                    break;
                                }
                            }
                        }
                    }
                }
                else if (items_to_check.at(0).get_name() == "inout")
                {
                    for (int i = 1; i < items_to_check.size(); i++)
                    {
                        for (TypeTag tt : find_in_items(items, items_to_check.at(i).get_name()))
                        {
                            switch (tt.get_class())
                            {
                            case resourceClass:
                                resources_produced.push_back(tt);
                                break;
                            case structureClass:
                                // Check for out_overlay
                                index = structures.at(tt.get_index()).find_outoverlay();
                                if (index >= 0)
                                {
                                    for (int i = index; i < structures.at(tt.get_index()).get_fields().size(); i++)
                                        if (!is_in_typeref(items_to_check, structures.at(tt.get_index()).get_fields().at(i).get_typeref().get_name()))
                                            items_to_check.push_back(structures.at(tt.get_index()).get_fields().at(i).get_typeref());
                                    break;
                                }

                                for (Field ff : structures.at(tt.get_index()).get_fields())
                                    if ((ff.has_attrs() && (ff.check_attrs("out") || ff.check_attrs("inout"))))
                                        items_to_check.push_back(ff.get_typeref());
                                break;
                            case typeolClass:
                                if (!is_in_typeref(items_to_check, typeols.at(tt.get_index()).get_type().get_name()))
                                    items_to_check.push_back(typeols.at(tt.get_index()).get_type());
                                break;
                            case typemlClass:
                                for (Field ff : typemls.at(tt.get_index()).get_fields())
                                {
                                    if (ff.has_attrs() && (ff.check_attrs("out") || ff.check_attrs("inout")))
                                    {
                                        // Keep track of passed args in type templates
                                        index = typemls.at(tt.get_index()).find_arg(ff.get_typeref().get_name());
                                        if (index >= 0)
                                            tmp = TypeRef(items_to_check.at(i).get_opts().at(index).get_name(), ff.get_typeref().get_opts());
                                        else
                                            tmp = ff.get_typeref();
                                        
                                        if (!is_in_typeref(items_to_check, tmp.get_name()))
                                            items_to_check.push_back(tmp);
                                    }
                                }
                                break;
                            default:
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (has_return())
        {
            index = find_in_items(items, TypeTag(resourceClass, return_type));
            if (index >= 0)
                resources_produced.push_back(items.at(index));
            else
                std::cerr << "Warning: Bad return value " << return_type << ".\n";
        }
        checked_produced = true;
    }
    return resources_produced;
}

int Syscall::total_resources(const std::vector<TypeTag> &items, const std::vector<TypeOneline> &typeols,
                                const std::vector<TypeMultiline> &typemls, const std::vector<Union> &unions,
                                const std::vector<Structure> &structures)
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
void item_push_sorted(std::vector<TypeTag> &items, const TypeTag &tt)
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

int find_in_items(const std::vector<TypeTag> &items, const TypeTag &query)
{
    if (items.empty())
        return -1;

    int l = 0, h = items.size() - 1, m;
    while (l <= h)
    {
        m = (l + h) / 2;
        if (query.get_name() == items.at(m).get_name() && query.get_class() == items.at(m).get_class())
            return m;
        else if (query.get_name() > items.at(m).get_name())
            l = m + 1;
        else
            h = m - 1;
    }

    // check for the exact class nearby
    for (int i = l; i >= 0 && i < items.size() && items.at(i).get_name() == query.get_name(); i++)
        l = items.at(i).get_class() == query.get_class() ? i : l;

    for (int i = l - 1; i >= 0 && i < items.size() && items.at(i).get_name() == query.get_name(); i--)
        l = items.at(i).get_class() == query.get_class() ? i : l;

    if (l >= items.size() || l < 0 || h < 0)
        return -1;

    return (items.at(l).get_name() == query.get_name() && query.get_class() == items.at(m).get_class() ? l : -1);
}

// Turns out that items of different type can have the same name.
// This function returns a vector of all items that share a given name
// for when you don't know which one you'll need.
// Thankfully, this really only happens when a type template is acting
// as a wrapper for flags
std::vector<TypeTag> find_in_items(const std::vector<TypeTag> &items, const std::string &query)
{
    std::vector<TypeTag> ret;
    if (items.empty())
        return ret;
    
    int l = 0, h = items.size() - 1, m;
    while (l <= h)
    {
        m = (l + h) / 2;
        if (query > items.at(m).get_name())
            l = m + 1;
        else
            h = m - 1;
    }

    for (int i = l; i >= 0 && i < items.size() && items.at(i).get_name() == query; i++)
        ret.push_back(items.at(i));

    for (int i = l - 1; i >= 0 && i < items.size() && items.at(i).get_name() == query; i--)
        ret.push_back(items.at(i));

    return ret;
}

// binary search on the items
bool is_in_items(const std::vector<TypeTag> &items, const TypeTag &query)
{
    return (find_in_items(items, query) >= 0);
}

// needed cannot be sorted because of how we add things to it.
bool is_in_needed(const std::vector<TypeTag> &needed, const TypeTag &query)
{
    for (TypeTag tt : needed)
        if (query.get_name() == tt.get_name() && query.get_class() == tt.get_class())
            return true;

    return false;
}

// linear search because sorting is a lot of effort for not a lot of time gained
bool is_in_includes(const std::vector<Include> &includes, const std::string &name)
{
    for (Include i : includes)
        if (i.get_name() == name)
            return true;
    return false;
}

bool is_in_typeref(const std::vector<TypeRef> &typerefs, const std::string &name)
{
    for (TypeRef tr : typerefs)
        if (tr.get_name() == name)
            return true;
    
    return false;
}

bool is_in_string(const std::vector<std::string> &strs, const std::string &s)
{
    for (std::string str : strs)
        if (s == str)
            return true;
    
    return false;
}

int find_in_syscalls(const std::vector<Syscall> &syscalls, const std::string &name)
{
    int i = 0;
    for (i = 0; i < syscalls.size(); i++)
        if (syscalls.at(i).get_name() == name)
            return i;
    
    return -1;
}

int stoi_custom(const std::string &line)
{
    int n = 0, neg = 1;
    if (line.empty())
        return -1;
    
    neg = (line.at(0) == '-' ? -1 : neg);

    for (int i = (neg < 0 ? 1 : 0); i < line.size(); i++)
        n = (n * 10) + (line.at(i) - '0');
    
    return n * neg;
}
