#include <template_parse.h>
#include <environment.h>
#include <file_api.h>
#include <syzlang.h>

#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <cctype>

// parses a typeref: name<[typeref]>
TypeRef parse_typeref(const std::string &typestring)
{
    TypeRef ret;
    int pos0 = 0, pos1 = typestring.find("["), count = 0;

    if (pos1 !=std::string::npos)
    { // if there are options
        ret.set_name(typestring.substr(pos0, pos1 - pos0));
        do {
            pos0 = pos1 + (typestring.at(pos1) == ',' ? 2 : 1);
            for (pos1 = pos1 + 1; pos1 < typestring.size(); pos1++)
            {
                if (typestring.at(pos1) == '[')
                    count++;
                else if (typestring.at(pos1) == ']' && count > 0)
                    count--;
                else if (typestring.at(pos1) == ',' && count == 0)
                    break;
                else if (typestring.at(pos1) == ']' && count == 0)
                    break;
            }
            ret.push_opt(parse_typeref(typestring.substr(pos0, pos1 - pos0)));
        } while (pos1 < typestring.size() && typestring.at(pos1) != ']');
    }
    else
        ret.set_name(typestring);

    return ret;
}

TailingAttribute parse_tailing_attribute(const std::string &line)
{
    TailingAttribute ta;
    int pos0 = 0, pos1 = line.find("[");

    if (pos1 !=std::string::npos)
    {
        ta.set_name(line.substr(pos0, pos1 - pos0));
        pos0 = pos1 + 1;
        pos1 = line.find("]", pos0);
        ta.set_n(stoi_custom(line.substr(pos0, pos1)));
    }
    else
    {
        ta.set_name(line);
        ta.set_n(-1);
    }

    return ta;
}

// parses a field: name typeref <(attr)>
Field parse_field(const std::string &line)
{
    Field field;
    int pos0 = line.find_first_not_of(" \t");
    int pos1 = line.find_first_of(" \t", pos0);
    if (pos0 ==std::string::npos || pos1 ==std::string::npos)
    {
        std::cerr << "Warning: Bad field " << line << ".\n";
        return field;
    }
    field.set_name(line.substr(pos0, pos1 - pos0));
    
    pos0 = line.find_first_not_of(" \t", pos1);
    if (pos0 ==std::string::npos)
    {
        std::cerr << "Warning: Bad field " << line << ".\n";
        return field;
    }

    pos1 = line.find("[", pos0);
    if (pos1 != std::string::npos && pos0 != std::string::npos)
        pos1 = line.find_last_of("]", pos0) + 1;
    else if (pos0 != std::string::npos)
        pos1 = line.find_first_of(" \t", pos0);

    if (pos1 != std::string::npos && pos0 != std::string::npos)
        field.set_typeref(parse_typeref(line.substr(pos0, pos1 - pos0)));
    else if (pos0 != std::string::npos)
        field.set_typeref(parse_typeref(line.substr(pos0)));

    pos1 = line.find("(", pos0);
    if (pos1 != std::string::npos)
    {
        do {
            pos0 = pos1 + (line.at(pos1) == ',' ? 2 : 1);
            pos1 = line.find_first_of(",)", pos0);
            field.push_attr(parse_tailing_attribute(line.substr(pos0, pos1 - pos0)));
        } while (line.at(pos1) != ')');
    }

    return field;
}

// parses a line that is known to be an include (or incdir) and pushes it to the given vector
void parse_include(std::vector<Include> &includes, const std::string &line)
{
    int pos0, pos1;
    std::string name;

    pos0 = line.find("<");
    pos1 = line.find(">");
    if (pos0 == std::string::npos || pos1 == std::string::npos)
    {
        std::cerr << "Error: Bad include: " << line << std::endl;
        return;
    }

    pos0++;
    name = line.substr(pos0, pos1 - pos0);

    if (!is_in_includes(includes, name))
        includes.push_back(Include(line, name));
    return;
}

// parses a line known to be a resource and pushes it to items and resources
void parse_resource(std::vector<TypeTag> &items, std::vector<Resource> &resources, const std::string &line)
{
    int pos0 = 9;
    int pos1 = line.find_first_of("[");
    std::string name, typestring;
    std::vector<std::string> sv;
    if (pos0 >= line.size() || pos1 == std::string::npos)
    {
        std::cerr << "Error: Bad resource: " << line << std::endl;
        return;
    }

    name = line.substr(pos0, pos1 - pos0);
    pos0 = pos1 + 1;
    pos1 = line.find_first_of("]");
    typestring = line.substr(pos0, pos1 - pos0);

    pos0 = line.find_first_of(":");
    if (pos0 != std::string::npos)
    {
        pos0 += 2;
        pos1 = line.find(",", pos0);
        while (pos1 != std::string::npos)
        {
            sv.push_back(line.substr(pos0, pos1 - pos0));
            pos0 = pos1 + 2;
            pos1 = line.find(",", pos0);
        }
        sv.push_back(line.substr(pos0));
    }

    if (!is_in_items(items, TypeTag(resourceClass, name)))
    {
        item_push_sorted(items, TypeTag(resourceClass, name, resources.size()));
        resources.push_back(Resource(line, name, parse_typeref(typestring), sv));
    }

    return;
}

// parses a multi-line type where all the lines are given in a vector
void parse_typeml(std::vector<TypeTag> &items, std::vector<TypeMultiline> &typemls, const std::vector<std::string> &lines)
{
    int pos0 = 5;
    int pos1 = lines.at(0).find_first_of(" [", pos0);
    std::string name; 
    std::vector<std::string> args;
    std::vector<Field> fields;

    if (pos1 == std::string::npos)
    {
        std::cerr << "Warning: Bad typeml " << lines.front() << ".\n";
        return;
    }
    name = lines.at(0).substr(pos0, pos1 - pos0);

    if (lines.at(0).at(pos1) == '[')
    { // parse type template args
        do {
            pos0 = pos1 + (lines.at(0).at(pos1) == ',' ? 2 : 1);
            pos1 = lines.at(0).find_first_of(",]", pos0);
            args.push_back(lines.at(0).substr(pos0, pos1 - pos0));
        } while (pos1 != std::string::npos && lines.at(0).at(pos1) != ']');
    }

    for (int i = 1; i < lines.size() - 1; i++)
        fields.push_back(parse_field(lines.at(i)));

    std::string text;
    for (std::string l : lines)
        text += l + "\n";

    if (!is_in_items(items, TypeTag(typemlClass, name)))
    {
        item_push_sorted(items, TypeTag(typemlClass, name, typemls.size()));
        typemls.push_back(TypeMultiline(text, name, args, fields));
    }

    return;
}

// parses a type that is only one line
void parse_typeol(std::vector<TypeTag> &items, std::vector<TypeOneline> &typeols, const std::string &line)
{
    int pos0 = 5;
    int pos1 = line.find_first_of(" [", pos0);
    std::string name; 
    std::vector<std::string> args;

    if (pos1 == std::string::npos)
    {
        std::cerr << "Warning: Bad typeol " << line << ".\n";
        return;
    }
    name = line.substr(pos0, pos1 - pos0);

    if (line.at(pos1) == '[')
    { // parse type template args
        do {
            pos0 = pos1 + (line.at(pos1) == ',' ? 2 : 1);
            pos1 = line.find_first_of(",]", pos0);
            args.push_back(line.substr(pos0, pos1 - pos0));
        } while (pos1 != std::string::npos && line.at(pos1) != ']');
    }

    // find the position of the underlying type
    pos0 = line.find_first_not_of(" ]", pos1);

    if (!is_in_items(items, TypeTag(typeolClass, name)) && pos0 != std::string::npos)
    {
        item_push_sorted(items, TypeTag(typeolClass, name, typeols.size()));
        typeols.push_back(TypeOneline(line, name, args, parse_typeref(line.substr(pos0))));
    }

    return;
}

void parse_definition(std::vector<TypeTag> &items, std::vector<Definition> &defines, const std::string &line)
{
    int pos0 = 7, pos1 = line.find_first_of(" \t", pos0);
    std::string name;

    if (pos1 == std::string::npos)
    {
        std::cerr << "Warning: Bad define " << line << ".\n";
        return;
    }
    name = line.substr(pos0, pos1 - pos0);

    if (!is_in_items(items, TypeTag(definitionClass, name)))
    {
        item_push_sorted(items, TypeTag(definitionClass, name, defines.size()));
        defines.push_back(Definition(line, name));
    }
    return;
}

void parse_syscall(std::vector<TypeTag> &items, std::vector<Syscall> &syscalls, const std::string &line)
{
    int pos0 = 0, pos1 = line.find("("), count = 0;
    Syscall syscall;
    
    if (pos1 == std::string::npos)
    {
        std::cerr << "Warning: Bad syscall " << line << ".\n";
        return;
    }
    syscall.set_name(line.substr(pos0, pos1 - pos0));
    syscall.set_text(line);

    do {
        pos0 = pos1 + (line.at(pos1) == ',' ? 2 : 1);
        for (pos1 = pos0; pos1 < line.size(); pos1++)
        {
            if (line.at(pos1) == '[')
                count++;
            else if (line.at(pos1) == ']' && count > 0)
                count--;
            else if (line.at(pos1) == ',' && count == 0)
                break;
            else if (line.at(pos1) == ')')
                break;
        }
        if (pos1 - pos0 != 0 && pos0 < line.size() && pos1 < line.size())
            syscall.push_field(parse_field(line.substr(pos0, pos1 - pos0)));
    } while (pos1 < line.size() && line.at(pos1) != ')');

    pos0 = line.find_first_not_of(" \t", pos1 + 1);

    if (pos0 != std::string::npos && line.at(pos0) != '(')
    {
        pos1 = line.find_first_of(" \t", pos0);
        if (pos1 != std::string::npos)
            syscall.set_return(line.substr(pos0, pos1 - pos0));
        else
            syscall.set_return(line.substr(pos0));
    }
    // tailing attributes have been left out for now

    if (!is_in_items(items, TypeTag(syscallClass, syscall.get_name())))
    {
        item_push_sorted(items, TypeTag(syscallClass, syscall.get_name(), syscalls.size()));
        syscalls.push_back(syscall);
    }
    return;
}

// Both struct and union are the same format, so we can use the same function to parse them
// heh, strunion
BaseStruct parse_strunion(const std::vector<std::string> &lines)
{
    BaseStruct bs;
    int pos0 = 0, pos1 = lines.front().find_first_of(" {[");
    if (pos1 == std::string::npos)
    {
        std::cerr << "Warning: Bad strunion " << lines.front() << ".\n";
    }

    bs.set_name(lines.front().substr(pos0, pos1 - pos0));

    for (int i = 1; i < lines.size() - 1; i++)
        bs.push_field(parse_field(lines.at(i)));

    std::string text;
    for (int i = 0; i < lines.size(); i++)
        text += lines.at(i) + (i == lines.size() - 1 ? "" : "\n");
    
    bs.set_text(text);

    return bs;
}

// parses a struct
void parse_structure(std::vector<TypeTag> &items, std::vector<Structure> &structures, const std::vector<std::string> &lines)
{
    BaseStruct bs = parse_strunion(lines);

    if (!is_in_items(items, TypeTag(structureClass, bs.get_name())))
    {
        item_push_sorted(items, TypeTag(structureClass, bs.get_name(), structures.size()));
        structures.push_back(Structure(bs));
    }
    return;
}

// parses a union
void parse_union(std::vector<TypeTag> &items, std::vector<Union> &unions, const std::vector<std::string> &lines)
{
    BaseStruct bs = parse_strunion(lines);

    if (!is_in_items(items, TypeTag(unionClass, bs.get_name())))
    {
        item_push_sorted(items, TypeTag(unionClass, bs.get_name(), unions.size()));
        unions.push_back(Union(bs));
    }
    return;
}

// parses a flag
void parse_flag(std::vector<TypeTag> &items, std::vector<Flag> &flags, const std::string &line)
{
    std::vector<std::string> values;
    std::string name;
    int pos0 = 0, pos1;
    
    pos1 = line.find(" = ");
    if (pos1 == std::string::npos)
    {
        std::cerr << "Warning: Bad flag " << line << ".\n";
        return;
    }
    name = line.substr(pos0, pos1 - pos0);

    do {
        pos0 = pos1 + (line.at(pos1) == ',' ? 2 : 3);
        pos1 = line.find(",", pos0);
        if (pos1 != std::string::npos)
            values.push_back(line.substr(pos0, pos1 - pos0));
        else
            values.push_back(line.substr(pos0));
    } while (pos1 != std::string::npos);

    if (!is_in_items(items, TypeTag(flagClass, name)))
    {
        item_push_sorted(items, TypeTag(flagClass, name, flags.size()));
        flags.push_back(Flag(line, name, values));
    }

    return;
}

void get_one_user_syscall(const TypeTag &this_resource, std::vector<TypeTag> &needed, const std::vector<TypeTag> &items,
                        std::vector<Syscall> &syscalls, std::vector<TypeOneline> &typeols, std::vector<TypeMultiline> &typemls,
                        std::vector<Union> &unions, std::vector<Structure> &structures)
{
    // check the syscalls already in needed
    int index;
    std::vector<TypeTag> used_recs;
    for (TypeTag tt : needed)
    {
        if (tt.get_class() == syscallClass)
        {
            used_recs.clear();
            used_recs = syscalls.at(tt.get_index()).get_resources_used(items, typeols, typemls, unions, structures);
            if (is_in_needed(used_recs, this_resource))
                return;
        }
    }

    Syscall chosen_syscall("", "");
    for (Syscall s : syscalls)
    {
        used_recs.clear();
        used_recs = s.get_resources_used(items, typeols, typemls, unions, structures);
        if (is_in_needed(used_recs, this_resource))
        {
            if (s.total_resources(items, typeols, typemls, unions, structures) == 1)
            {
                chosen_syscall = s;
                break;
            }
            else if (chosen_syscall.get_text().empty() ||
                        s.total_resources(items, typeols, typemls, unions, structures)
                        < chosen_syscall.total_resources(items, typeols, typemls, unions, structures))
                chosen_syscall = s;
        }
    }

    if (!chosen_syscall.get_text().empty())
    {
        index = find_in_items(items, TypeTag(syscallClass, chosen_syscall.get_name()));
        needed.push_back(items.at(index));
    }
    else
        std::cerr << "Warning: No syscall found that uses " << this_resource.get_name() << ".\n";
    return;
}

void get_one_producer_syscall(const TypeTag &this_resource, std::vector<TypeTag> &needed, const std::vector<TypeTag> &items,
                        std::vector<Syscall> &syscalls, const std::vector<TypeOneline> &typeols, const std::vector<TypeMultiline> &typemls,
                        const std::vector<Union> &unions, const std::vector<Structure> &structures)
{
    int index;
    // Manually check for some resources that are difficult for one reason or another.
    std::map<std::string, std::string> problematic = {
        {"uid", "getuid"},
        {"gid", "getgid"},
        {"fd_perf_base", "perf_event_open"},
        {"fd_bpf_token", "bpf$TOKEN_CREATE"},
        {"fd_bpf_const_str", "bpf$MAP_UPDATE_CONST_STR"},
        {"tail_call_map", "bpf$MAP_UPDATE_ELEM_TAIL_CALL"},
        {"fd_bpf_prog", "bpf$BPF_PROG_GET_FD_BY_ID"},
        {"assoc_id", "getsockopt"}, //$inet_sctp6_SCTP_AUTH_ACTIVE_KEY
        {"fd_dir", "open$dir"},
    };

    if (problematic.count(this_resource.get_name()) == 1)
    {
        index = find_in_items(items, TypeTag(syscallClass, problematic.at(this_resource.get_name())));
        if (!is_in_needed(needed, items.at(index)))
            needed.push_back(items.at(index));
        return;
    }

    // check the syscalls already in needed
    std::vector<TypeTag> produced_recs;
    for (TypeTag tt : needed)
    {
        if (tt.get_class() == syscallClass)
        {
            produced_recs.clear();
            produced_recs = syscalls.at(tt.get_index()).get_resources_produced(items, typeols, typemls, unions, structures);
            if (is_in_needed(produced_recs, this_resource))
                return;
        }
    }

    Syscall chosen_syscall("", "");
    for (Syscall s : syscalls)
    {
        produced_recs.clear();
        produced_recs = s.get_resources_produced(items, typeols, typemls, unions, structures);
        if (is_in_needed(produced_recs, this_resource))
        {
            if (s.total_resources(items, typeols, typemls, unions, structures) == 1)
            {
                chosen_syscall = s;
                break;
            }
            else if (chosen_syscall.get_text().empty() ||
                    s.total_resources(items, typeols, typemls, unions, structures)
                    < chosen_syscall.total_resources(items, typeols, typemls, unions, structures))
            {
                chosen_syscall = s;
            }
        }
    }

    if (!chosen_syscall.get_text().empty())
    {
        index = find_in_items(items, TypeTag(syscallClass, chosen_syscall.get_name()));
        needed.push_back(items.at(index));
    }
    else
        std::cerr << "Warning: No syscall found that produces " << this_resource.get_name() << ".\n";
    return;
}

void push_syscall_depends(std::vector<Syscall> &syscalls, int index, std::vector<TypeTag> &needed, std::vector<TypeTag> &items)
{
    syscalls.at(index).push_depends(needed, items);

    // It seems we are not finding resources for some syscalls. Add them manually.
    std::map<std::string, std::vector<TypeTag>> problematic = {
        {"bpf$BPF_PROG_RAW_TRACEPOINT_LOAD", {TypeTag(resourceClass, "tail_call_map")}},
        {"bpf$BPF_BTF_LOAD", {TypeTag(syscallClass, "bpf$TOKEN_CREATE")}},
        {"setsockopt$inet_sctp6_SCTP_AUTH_KEY", {TypeTag(resourceClass, "assoc_id")}},
        {"bpf$TOKEN_CREATE", {TypeTag(resourceClass, "fd_bpf_prog")}},
        {"bpf$BPF_PROG_DETACH", {TypeTag(resourceClass, "fd_cgroup"), TypeTag(resourceClass, "fd_bpf_link")}},
    };

    if (problematic.count(syscalls.at(index).get_name()) == 1)
    {
        for (TypeTag tag : problematic.at(syscalls.at(index).get_name()))
        {
            index = find_in_items(items, tag);
            if (!is_in_needed(needed, items.at(index)))
                needed.push_back(items.at(index));
        }
    }
}

std::vector<std::string> slim_template(Environment &env, const std::string &full_template)
{
    // Wow this is all bad. But is works (mostly) and I don't want to touch it.
    int pos0, pos1, index;

    std::ifstream templateIn;
    std::ifstream reproIn;
    std::ofstream outf;
    std::string line, line2;
    std::vector<std::string> lines;

    std::vector<TypeTag> items;
    std::vector<Include> includes;
    std::vector<Resource> resources;
    std::vector<TypeOneline> typeols;
    std::vector<TypeMultiline> typemls;
    std::vector<Definition> definitions;
    std::vector<Syscall> syscalls;
    std::vector<Structure> structures;
    std::vector<Union> unions;
    std::vector<Flag> flags;

    std::vector<TypeTag> needed;
    std::vector<std::string> ret;

    // ======================================================================================================
    // Read the reproducer to get syscalls

    // For each repro
    std::vector<std::string> reproducers = list_dir(env.reprodir);
    for (std::string file : reproducers)
    {
        reproIn.open(file);
        if(!reproIn)
        {
            std::cerr << "Failed to open reproducer file!" << file << std::endl;
            return ret;
        }

        line.clear();
        while (getline(reproIn, line))
        {
            if (line.empty() || line.at(0) == '#')
                continue;

            pos0 = line.find(" = ");
            pos0 = (pos0 == std::string::npos) ? 0 : pos0 + 3;
            pos1 = line.find("(");

            if (!is_in_string(env.base_syscalls, line.substr(pos0, pos1 - pos0)))
                env.base_syscalls.push_back(line.substr(pos0, pos1 - pos0));
            line.clear();
        }
        reproIn.close();
    }

    bool hasauto = false;
    for (std::string s : env.base_syscalls)
    {
        if (s.find("auto") != std::string::npos)
        {
            hasauto = true;
            break;
        }
    }
    std::vector<std::string> templateFiles = list_template_files(full_template, hasauto);

    // ======================================================================================================
    // Parse the full template
    for (std::string filename : templateFiles)
    {
        templateIn.open(filename);
        if (!templateIn)
        {
            std::cout << "Error: Failed to open file " << filename << ".\n";
            return ret;
        }

        line.clear();
        lines.clear();
        while (getline(templateIn, line))
        {
            // __structname is a case now I guess
            if (line.empty() || line.at(0) == '#' || (line.at(0) == '_' && line.at(1) != '_') || line.substr(0, 5) == "meta ")
            { // skip empty lines, commments, unnamed vars, and meta
                line.clear();
                continue;
            }
            else if (line.substr(0, 8) == "include " || line.substr(0, 7) == "incdir ")
            { // if it is an include or incdir
                parse_include(includes, line);
            }
            else if (line.substr(0, 9) == "resource ")
            { // if it is a resource
                parse_resource(items, resources, line);
            }
            else if (line.substr(0, 5) == "type ")
            { // if it is a type
                if (line.at(line.size() - 1) == '{' || line.at(line.size() - 1) == '[')
                { // check for multi-line type
                    do {
                        if (line.empty() || line.at(0) == '#')
                            continue;
                        lines.push_back(line);
                        if (line.at(0) == '}' || line.at(0) == ']')
                            break;
                    } while (getline(templateIn, line));
                    parse_typeml(items, typemls, lines);
                }
                else
                {
                    parse_typeol(items, typeols, line);
                }
            }
            else if (line.substr(0, 7) == "define ")
            { // if it is a define
                parse_definition(items, definitions, line);
            }
            else if (line.find("(") != std::string::npos && line.find(" = ") == std::string::npos)
            { // if it is a syscall
                parse_syscall(items, syscalls, line);
            }
            else if (line.at(line.size() - 1) == '{')
            { // if it is a struct
                do {
                    if (line.empty() || line.at(0) == '#')
                        continue;
                    lines.push_back(line);
                    if (line.at(0) == '}')
                        break;
                } while (getline(templateIn, line));

                parse_structure(items, structures, lines);
            }
            else if (line.at(line.size() - 1) == '[')
            { // if it is a union
                do {
                    if (line.empty() || line.at(0) == '#')
                        continue;
                    lines.push_back(line);
                    if (line.at(0) == ']')
                        break;
                } while (getline(templateIn, line));

                parse_union(items, unions, lines);
            }
            else if (line.find(" = ") != std::string::npos)
            { // if it is a flag
                parse_flag(items, flags, line);
            }
            else {
                std::cerr << "Warning: Unknown line type in " << filename << ": " << line << std::endl;
            }
            line.clear();
            lines.clear();
        }

        templateIn.close();
    }

    // ======================================================================================================
    // Create the slimmed template
    // grab everything first, then print it all to the file

    // initial syscalls
    for (std::string s : env.base_syscalls)
    {
        index = find_in_syscalls(syscalls, s);
        if (!is_in_needed(needed, TypeTag(syscallClass ,s)) && index >= 0)
            item_push_sorted(needed, TypeTag(syscallClass, s, index));
    }

    // Demote resources down to basic types
    // Special values add on
    // Skip this idea for now

    // For each item that is needed, grab all of its depends as well
    for (int i = 0; i < needed.size(); i++)
    {
        index = needed.at(i).get_index();
        if (index < 0)
        {
            std::cerr << "Warning: Unknown item " << needed.at(i).get_name() << ".\n";
            continue;
        }
        switch (needed.at(i).get_class())
        {
        case resourceClass:
            resources.at(index).push_depends(needed, items);
            if (!resources.at(index).has_depends())
            {
                get_one_user_syscall(needed.at(i), needed, items, syscalls, typeols, typemls, unions, structures);
            }
            get_one_producer_syscall(needed.at(i), needed, items, syscalls, typeols, typemls, unions, structures);
            break;
        case typeolClass:
            typeols.at(index).push_depends(needed, items);
            break;
        case typemlClass:
            typemls.at(index).push_depends(needed, items);
            break;
        case definitionClass:
            //definitions.at(index).push_depends(needed, items);
            break;
        case unionClass:
            unions.at(index).push_depends(needed, items);
            break;
        case structureClass:
            structures.at(index).push_depends(needed, items);
            break;
        case flagClass:
            flags.at(index).push_depends(needed, items);
            break;
        case syscallClass:
            push_syscall_depends(syscalls, index, needed, items);
            break;
        default:
            std::cerr << "Warning: Unknown class type found while grabbing dependencies.\n";
            break;
        };
    }

    for (TypeTag tt : needed)
    {
        //index = tt.get_index();
        if (tt.get_class() == syscallClass)
        {
            /*syscalls.at(index).get_name()*/
            ret.push_back(tt.get_name());
        }
    }

    return ret;
}

std::vector<std::string> list_template_files(const std::string &template_dir, bool hasauto)
{
    std::vector<std::string> files = list_dir(template_dir);

    // traversing backwards so we don't mess up as we delete
    for (int i = files.size() - 1; i >= 0; i--)
        if (files.at(i).find(".const") != std::string::npos || files.at(i).find(".warn") != std::string::npos
        || files.at(i).find(".info") != std::string::npos || (!hasauto && files.at(i).find("auto") != std::string::npos)
        || files.at(i).find(".txt") == std::string::npos)
            files.erase(files.begin() + i);

    return files;
}

std::vector<std::string> get_reproducer_syscall_descriptions(Environment &env)
{
    std::string full_template = env.syzdir + "sys/linux";
    std::vector<std::string> syscalls = slim_template(env, full_template);
    if (syscalls.size() == 0)
    {
        std::cerr << "Error: failed to slim the template.\n";
        return {};
    }
    return syscalls;
}
