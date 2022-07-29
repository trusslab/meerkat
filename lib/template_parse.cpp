#include <template_parse.h>
#include <file_api.h>
#include <syzlang.h>

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cctype>

using namespace std;

const bool VERB = false;

// parses a typeref: name<[typeref]>
TypeRef parse_typeref(const string &typestring)
{
    TypeRef ret;
    int pos0 = 0, pos1 = typestring.find("["), count = 0;

    if (pos1 != string::npos)
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

TailingAttribute parse_tailing_attribute(const string &line)
{
    TailingAttribute ta;
    int pos0 = 0, pos1 = line.find("[");

    if (pos1 != string::npos)
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
Field parse_field(const string &line)
{
    Field field;
    int pos0 = line.find_first_not_of(" \t");
    int pos1 = line.find_first_of(" \t", pos0);
    if (pos0 == string::npos || pos1 == string::npos)
    {
        cerr << "Warning: Bad field " << line << ".\n";
        return field;
    }
    field.set_name(line.substr(pos0, pos1 - pos0));
    
    pos0 = line.find_first_not_of(" \t", pos1);
    if (pos0 == string::npos)
    {
        cerr << "Warning: Bad field " << line << ".\n";
        return field;
    }

    pos1 = line.find("[", pos0);
    if (pos1 != string::npos && pos0 != string::npos)
        pos1 = line.find_last_of("]", pos0) + 1;
    else if (pos0 != string::npos)
        pos1 = line.find_first_of(" \t", pos0);

    if (pos1 != string::npos && pos0 != string::npos)
        field.set_typeref(parse_typeref(line.substr(pos0, pos1 - pos0)));
    else if (pos0 != string::npos)
        field.set_typeref(parse_typeref(line.substr(pos0)));

    pos1 = line.find("(", pos0);
    if (pos1 != string::npos)
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
void parse_include(vector<Include> &includes, const string &line)
{
    int pos0, pos1;
    string name;

    pos0 = line.find("<");
    pos1 = line.find(">");
    if (pos0 == string::npos || pos1 == string::npos)
    {
        cerr << "Error: Bad include: " << line << endl;
        return;
    }

    pos0++;
    name = line.substr(pos0, pos1 - pos0);

    if (!is_in_includes(includes, name))
        includes.push_back(Include(line, name));
    return;
}

// parses a line known to be a resource and pushes it to items and resources
void parse_resource(vector<TypeTag> &items, vector<Resource> &resources, const string &line)
{
    int pos0 = 9;
    int pos1 = line.find_first_of("[");
    string name, typestring;
    vector<string> sv;
    if (pos0 >= line.size() || pos1 == string::npos)
    {
        cerr << "Error: Bad resource: " << line << endl;
        return;
    }

    name = line.substr(pos0, pos1 - pos0);
    pos0 = pos1 + 1;
    pos1 = line.find_first_of("]");
    typestring = line.substr(pos0, pos1 - pos0);

    pos0 = line.find_first_of(":");
    if (pos0 != string::npos)
    {
        pos0 += 2;
        pos1 = line.find(",", pos0);
        while (pos1 != string::npos)
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
void parse_typeml(vector<TypeTag> &items, vector<TypeMultiline> &typemls, const vector<string> &lines)
{
    int pos0 = 5;
    int pos1 = lines.at(0).find_first_of(" [", pos0);
    string name; 
    vector<string> args;
    vector<Field> fields;

    if (pos1 == string::npos)
    {
        cerr << "Warning: Bad typeml " << lines.front() << ".\n";
        return;
    }
    name = lines.at(0).substr(pos0, pos1 - pos0);

    if (lines.at(0).at(pos1) == '[')
    { // parse type template args
        do {
            pos0 = pos1 + (lines.at(0).at(pos1) == ',' ? 2 : 1);
            pos1 = lines.at(0).find_first_of(",]", pos0);
            args.push_back(lines.at(0).substr(pos0, pos1 - pos0));
        } while (pos1 != string::npos && lines.at(0).at(pos1) != ']');
    }

    for (int i = 1; i < lines.size() - 1; i++)
        fields.push_back(parse_field(lines.at(i)));

    string text;
    for (string l : lines)
        text += l + "\n";

    if (!is_in_items(items, TypeTag(typemlClass, name)))
    {
        item_push_sorted(items, TypeTag(typemlClass, name, typemls.size()));
        typemls.push_back(TypeMultiline(text, name, args, fields));
    }

    return;
}

// parses a type that is only one line
void parse_typeol(vector<TypeTag> &items, vector<TypeOneline> &typeols, const string &line)
{
    int pos0 = 5;
    int pos1 = line.find_first_of(" [", pos0);
    string name; 
    vector<string> args;

    if (pos1 == string::npos)
    {
        cerr << "Warning: Bad typeol " << line << ".\n";
        return;
    }
    name = line.substr(pos0, pos1 - pos0);

    if (line.at(pos1) == '[')
    { // parse type template args
        do {
            pos0 = pos1 + (line.at(pos1) == ',' ? 2 : 1);
            pos1 = line.find_first_of(",]", pos0);
            args.push_back(line.substr(pos0, pos1 - pos0));
        } while (pos1 != string::npos && line.at(pos1) != ']');
    }

    // find the position of the underlying type
    pos0 = line.find_first_not_of(" ]", pos1);

    if (!is_in_items(items, TypeTag(typeolClass, name)) && pos0 != string::npos)
    {
        item_push_sorted(items, TypeTag(typeolClass, name, typeols.size()));
        typeols.push_back(TypeOneline(line, name, args, parse_typeref(line.substr(pos0))));
    }

    return;
}

void parse_definition(vector<TypeTag> &items, vector<Definition> &defines, const string &line)
{
    int pos0 = 7, pos1 = line.find_first_of(" \t", pos0);
    string name;

    if (pos1 == string::npos)
    {
        cerr << "Warning: Bad define " << line << ".\n";
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

void parse_syscall(vector<TypeTag> &items, vector<Syscall> &syscalls, const string &line)
{
    int pos0 = 0, pos1 = line.find("("), count = 0;
    Syscall syscall;
    
    if (pos1 == string::npos)
    {
        cerr << "Warning: Bad syscall " << line << ".\n";
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

    if (pos0 != string::npos && line.at(pos0) != '(')
    {
        pos1 = line.find_first_of(" \t", pos0);
        if (pos1 != string::npos)
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
BaseStruct parse_strunion(const vector<string> &lines)
{
    BaseStruct bs;
    int pos0 = 0, pos1 = lines.front().find_first_of(" {[");
    if (pos1 == string::npos)
    {
        cerr << "Warning: Bad strunion " << lines.front() << ".\n";
    }

    bs.set_name(lines.front().substr(pos0, pos1 - pos0));

    for (int i = 1; i < lines.size() - 1; i++)
        bs.push_field(parse_field(lines.at(i)));

    string text;
    for (int i = 0; i < lines.size(); i++)
        text += lines.at(i) + (i == lines.size() - 1 ? "" : "\n");
    
    bs.set_text(text);

    return bs;
}

// parses a struct
void parse_structure(vector<TypeTag> &items, vector<Structure> &structures, const vector<string> &lines)
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
void parse_union(vector<TypeTag> &items, vector<Union> &unions, const vector<string> &lines)
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
void parse_flag(vector<TypeTag> &items, vector<Flag> &flags, const string &line)
{
    vector<string> values;
    string name;
    int pos0 = 0, pos1;
    
    pos1 = line.find(" = ");
    if (pos1 == string::npos)
    {
        cerr << "Warning: Bad flag " << line << ".\n";
        return;
    }
    name = line.substr(pos0, pos1 - pos0);

    do {
        pos0 = pos1 + (line.at(pos1) == ',' ? 2 : 3);
        pos1 = line.find(",", pos0);
        if (pos1 != string::npos)
            values.push_back(line.substr(pos0, pos1 - pos0));
        else
            values.push_back(line.substr(pos0));
    } while (pos1 != string::npos);

    if (!is_in_items(items, TypeTag(flagClass, name)))
    {
        item_push_sorted(items, TypeTag(flagClass, name, flags.size()));
        flags.push_back(Flag(line, name, values));
    }

    return;
}

void get_one_user_syscall(const TypeTag &this_resource, vector<TypeTag> &needed, const vector<TypeTag> &items,
                        vector<Syscall> &syscalls, vector<TypeOneline> &typeols, vector<TypeMultiline> &typemls,
                        vector<Union> &unions, vector<Structure> &structures)
{
    // check the syscalls already in needed
    int index;
    vector<TypeTag> used_recs;
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
        cerr << "Warning: No syscall found that uses " << this_resource.get_name() << ".\n";
    return;
}

void get_one_producer_syscall(const TypeTag &this_resource, vector<TypeTag> &needed, const vector<TypeTag> &items,
                        vector<Syscall> &syscalls, const vector<TypeOneline> &typeols, const vector<TypeMultiline> &typemls,
                        const vector<Union> &unions, const vector<Structure> &structures)
{
    // check the syscalls already in needed
    int index;
    vector<TypeTag> produced_recs;
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
        cerr << "Warning: No syscall found that produces " << this_resource.get_name() << ".\n";
    return;
}

int slim_template(const string &reproFile, const string &outfilename, const vector<string> &templateFiles)
{
    int pos0, pos1, index;

    ifstream templateIn;
    ifstream reproIn;
    ofstream outf;
    string line, line2;
    vector<string> lines;

    vector<TypeTag> items;
    vector<Include> includes;
    vector<Resource> resources;
    vector<TypeOneline> typeols;
    vector<TypeMultiline> typemls;
    vector<Definition> definitions;
    vector<Syscall> syscalls;
    vector<Structure> structures;
    vector<Union> unions;
    vector<Flag> flags;

    vector<TypeTag> needed;

    // ======================================================================================================
    // Parse the full template
    for (string filename : templateFiles)
    {
        templateIn.open(filename);
        if (!templateIn)
        {
            cout << "Error: Failed to open file " << filename << ".\n";
            return -1;
        }

        if (VERB)
            cout << "Parsing " << filename << ".\n";

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
                if (VERB)
                    cout << "Found include.\n";
                parse_include(includes, line);
            }
            else if (line.substr(0, 9) == "resource ")
            { // if it is a resource
                if (VERB)
                    cout << "Found resource.\n";
                parse_resource(items, resources, line);
            }
            else if (line.substr(0, 5) == "type ")
            { // if it is a type
                if (line.at(line.size() - 1) == '{' || line.at(line.size() - 1) == '[')
                { // check for multi-line type
                    if (VERB)
                        cout << "Found multi-line type.\n";
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
                    if (VERB)
                        cout << "Found one-line type.\n";
                    parse_typeol(items, typeols, line);
                }
            }
            else if (line.substr(0, 7) == "define ")
            { // if it is a define
                if (VERB)
                    cout << "Found definition.\n";
                
                parse_definition(items, definitions, line);
            }
            else if (line.find("(") != string::npos)
            { // if it is a syscall
                if (VERB)
                    cout << "Found syscall.\n";

                parse_syscall(items, syscalls, line);
            }
            else if (line.at(line.size() - 1) == '{')
            { // if it is a struct
                if (VERB)
                    cout << "Found structure.\n";

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
                if (VERB)
                    cout << "Found union.\n";
                do {
                    if (line.empty() || line.at(0) == '#')
                        continue;
                    lines.push_back(line);
                    if (line.at(0) == ']')
                        break;
                } while (getline(templateIn, line));

                parse_union(items, unions, lines);
            }
            else if (line.find(" = ") != string::npos)
            { // if it is a flag
                if (VERB)
                    cout << "Found flag.\n";
                
                parse_flag(items, flags, line);
            }
            else {
                cerr << "Warning: Unknown line type: " << line << endl;
            }
            line.clear();
            lines.clear();
        }

        templateIn.close();
    }

    // ======================================================================================================
    // Read the reproducer to get syscalls
    reproIn.open(reproFile);
    if(!reproIn)
    {
        cout << "Failed to open reproducer file!" << reproFile << endl;
        return -1;
    }

    // Always include a few syscalls that are needed for syzkaller to function.
    vector<string> reproducer_syscalls = {"syz_execute_func", "mmap", "open"};

    line.clear();
    while (getline(reproIn, line))
    {
        if (line.empty() || line.at(0) == '#')
            continue;

        pos0 = line.find(" = ");
        pos0 = (pos0 == string::npos) ? 0 : pos0 + 3;
        pos1 = line.find("(");

        if (!is_in_string(reproducer_syscalls, line.substr(pos0, pos1 - pos0)))
            reproducer_syscalls.push_back(line.substr(pos0, pos1 - pos0));
        line.clear();
    }
    reproIn.close();

    // ======================================================================================================
    // Create the slimmed template
    // grab everything first, then print it all to the file

    // initial syscalls
    for (string s : reproducer_syscalls)
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
            cerr << "Warning: Unknown item " << needed.at(i).get_name() << ".\n";
            continue;
        }
        switch (needed.at(i).get_class())
        {
        case resourceClass:
            resources.at(index).push_depends(needed, items);
            get_one_user_syscall(needed.at(i), needed, items, syscalls, typeols, typemls, unions, structures);
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
            syscalls.at(index).push_depends(needed, items);
            break;
        default:
            cout << "Warning: Unknown class type found while grabbing dependencies.\n";
            break;
        };
    }

    // ======================================================================================================
    // Print the template to the file

    outf.open(outfilename);
    if (!outf)
    {
        cout << "Failed to open output file: " << outfilename << endl;
        return -1;
    }

    for (Include i : includes)
        outf << i.get_text() << endl;

    outf << "\n\n";

    for (TypeTag tt : needed)
    {
        index = tt.get_index();
        switch (tt.get_class())
        {
        case resourceClass:
            outf << resources.at(index).get_text() << "\n\n";
            break;
        case typeolClass:
            outf << typeols.at(index).get_text() << "\n\n";
            break;
        case typemlClass:
            outf << typemls.at(index).get_text() << "\n\n";
            break;
        case definitionClass:
            outf << definitions.at(index).get_text() << "\n\n";
            break;
        case unionClass:
            outf << unions.at(index).get_text() << "\n\n";
            break;
        case structureClass:
            outf << structures.at(index).get_text() << "\n\n";
            break;
        case flagClass:
            outf << flags.at(index).get_text() << "\n\n";
            break;
        case syscallClass:
            outf << syscalls.at(index).get_text() << "\n\n";
            break;
        default:
            cout << "Warning: Unknown class type found while printing.\n";
            break;
        };
    }

    outf << flush;
    outf.close();
    return 0;
}

vector<string> list_template_files(const string &template_dir)
{
    vector<string> files = list_dir(template_dir);

    // traversing backwards so we don't mess up as we delete
    for (int i = files.size() - 1; i >= 0; i--)
        if (files.at(i).find(".const") != string::npos || files.at(i).find(".warn") != string::npos || files.at(i).find(".txt") == string::npos)
            files.erase(files.begin() + i);

    return files;
}

int remove_template_files(const vector<string> &template_files)
{
    for (string file : template_files)
        remove_file(file);

    return 0;
}

bool compare_templates(const string &template1, const string &template2)
{
    bool ok1, ok2, ret = true;
    string line1, line2;
    ifstream inf1, inf2;
    inf1.open(template1);
    inf2.open(template2);

    if (!inf1 || !inf2)
    {
        cerr << "Error: Failed to open either " << template1 << " or " << template2 << ".\n";
        return false;
    }

    // skip all of the includes
    while (getline(inf1, line1) && line1.substr(0, 7) == "include");
    while (getline(inf2, line2) && line2.substr(0, 7) == "include");

    while (true)
    {
        ok1 = getline(inf1, line1) ? true : false;
        ok2 = getline(inf2, line2) ? true : false;

        if (ok1 && ok2)
        {
            if (line1 != line2)
            {
                ret = false;
                break;
            }
        }
        else if (!ok1 && !ok2)
        {
            break;
        }
        else
        {
            ret = false;
            break;
        }
    }

    inf1.close();
    inf2.close();
    return ret;
}
