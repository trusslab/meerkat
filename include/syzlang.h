#ifndef STRUCTS_H
#define STRUCTS_H
#include <string>
#include <vector>

enum ParseClass { resourceClass, typeolClass, typemlClass, definitionClass, unionClass, structureClass, flagClass, syscallClass, includeClass };
/*
const std::vector<std::string> KEYWORDS = {"in", "out", "int64", "int32", "int16", "int8", "intptr", "len", "opt", "filename", "array", "const", "ptr64",
                                "string", "flags", "bytesize", "stringnoz", "parent", "inout", "sizeof", "timeout", "ptr", "bool8", "bool16", "bool32",
                                "bool64", "fmt", "void", "int16be", "int8be", "int32be", "int64be", "glob", "bitsize", "offsetof", "vma", "vma64", "proc",
                                "text", "bytesize4", "bytesize8", "bytesize16", "bytesize32", "bytesize64", "dec", "oct", "hex", "align", "packed", "size",
                                "varlen"};
const std::vector<std::string> SYSCALL_ATTRIBUTES = {"disabled", "timeout", "prog_timeout", "ignore_return", "breaks_returns"};
const std::vector<std::string> STRUCT_ATTRIBUTES = {"packed", "align", "size"};
const std::vector<std::string> UNION_ATTRIBUTES = {"varlen", "size"};
*/

// ======================================================================================================
// Tag

class TypeTag
{
private:
    std::string name;
    ParseClass pc;
    int index;

public:
    TypeTag()
    { return; }

    TypeTag(const ParseClass &pclass, const std::string &n, int i = -1)
        : name(n), pc(pclass), index(i)
    { return; }

    TypeTag(const TypeTag &t)
    {
        pc = t.pc;
        name = t.name;
        index = t.index;
        return;
    }

    ParseClass get_class() const;
    std::string get_name() const;
    int get_index() const;

    void set_type(const ParseClass &, const std::string &);
};

// ======================================================================================================
// Base class

class Identifier
{
protected:
    std::string text;
    std::string name;
    std::vector<TypeTag> depends;
    bool checked_depends;

public:
    Identifier()
        : checked_depends(false)
    { return; }

    Identifier(const std::string &t, const std::string &n)
        : text(t), name(n), checked_depends(false)
    { return; }

    std::string get_name() const;
    void set_name(const std::string &);

    std::string get_text() const;
    void set_text(const std::string &);
};

// ======================================================================================================
// Member Classes

// typename <[type-options]>
class TypeRef
{
private:
    std::string name;
    std::vector<TypeRef> options;
    std::vector<TypeTag> depends;
    bool checked_depends;

public:
    TypeRef()
        : checked_depends(false)
    { return; }

    TypeRef(const std::string &n, const std::vector<TypeRef> &opt = {})
        : name(n), options(opt), checked_depends(false)
    { return; }

    TypeRef(const TypeRef &t)
        : name(t.get_name()), checked_depends(false)
    {
        options = t.options;
        return;
    }

    std::string print() const;

    std::string get_name() const;
    void set_name(const std::string &);

    bool has_opts() const;
    void push_opt(const TypeRef &);
    std::vector<TypeRef> get_opts() const;

    void push_depends(std::vector<TypeTag> &, const std::vector<TypeTag> &);
};

// name<[n]>
class TailingAttribute
{
private:
    std::string name;
    int n;

public:
    TailingAttribute()
    { return; }

    TailingAttribute(const std::string &na, int x = -1)
        : name(na), n(x)
    { return; }

    std::string print() const;

    std::string get_name() const;
    void set_name(const std::string &);

    bool has_n() const;
    int get_n() const;
    void set_n(int);
};

class Field
{
protected:
    std::string name;
    TypeRef type;
    std::vector<TailingAttribute> attributes;
    std::vector<TypeTag> depends;
    bool checked_depends;

public:
    Field()
        : checked_depends(false)
    { return; }

    Field(const std::string &n, const TypeRef &t, const std::vector<TailingAttribute> &attr = {})
        : name(n), type(t), attributes(attr), checked_depends(false)
    { return; }

    std::string print() const;

    std::string get_name() const;
    void set_name(const std::string &);

    TypeRef& get_typeref();
    void set_typeref(const TypeRef &);

    bool has_attrs() const;
    void push_attr(const TailingAttribute &);
    bool check_attrs(const std::string &) const;

    void push_depends(std::vector<TypeTag> &, const std::vector<TypeTag> &);
};

// ======================================================================================================
// Other directives

// define NAME value
class Definition : public Identifier
{
public:
    Definition()
        : Identifier()
    { return; }

    Definition(const std::string &t, const std::string &n)
        : Identifier(t, n)
    { return; }

    std::string print() const;

    void push_depends(std::vector<TypeTag> &, const std::vector<TypeTag> &);
};

// include <filename>
// incdir <directory>
class Include : public Identifier
{
public:
    Include()
        : Identifier()
    { return; }

    Include(const std::string &t, const std::string &n)
        : Identifier(t, n)
    { return; }

    std::string print() const;
};

// ======================================================================================================
// Types

class BaseType : public Identifier
{
protected:
    std::vector<std::string> args;

public:
    BaseType()
        : Identifier()
    { return; }

    BaseType(const std::string &t, const std::string &n, const std::vector<std::string> &a = {})
        : Identifier(t, n), args(a)
    { return; }

    std::string print_args() const;

    void push_arg(const std::string &);
    std::vector<std::string>& get_args();
    bool has_args() const;
    int find_arg(const std::string &) const;
};

// type identifier underlying_type
class TypeOneline : public BaseType
{
private:
    TypeRef type;

public:
    TypeOneline()
        : BaseType()
    { return; }

    TypeOneline(const std::string &t, const std::string &n, const std::vector<std::string> &a = {}, const TypeRef &ty = TypeRef())
        : BaseType(t, n, a), type(ty)
    { return; }

    std::string print() const;

    TypeRef get_type() const;

    void push_depends(std::vector<TypeTag> &, const std::vector<TypeTag> &);
};

/*
type nlattr[TYPE, PAYLOAD] {
	nla_len		len[parent, int16]
	nla_type	const[TYPE, int16]
	payload		PAYLOAD
} [align_4]
*/
class TypeMultiline : public BaseType
{
private:
    std::vector<Field> fields;

public:
    TypeMultiline()
        : BaseType()
    { return; }

    TypeMultiline(const std::string &t, const std::string &n, const std::vector<std::string> &a = {}, const std::vector<Field> &f = {})
        : BaseType(t, n, a), fields(f)
    { return; }

    std::string print() const;

    std::vector<Field> get_fields() const;

    void push_depends(std::vector<TypeTag> &, const std::vector<TypeTag> &);
};

// ======================================================================================================
// Resource

// "resource" identifier "[" underlying_type "]" [ ":" const ("," const)* ]
class Resource : public Identifier
{
private:
    TypeRef type;
    std::vector<std::string> special_values;

public:
    Resource()
        : Identifier()
    { return; }

    Resource(const std::string &t, const std::string &n, const TypeRef &tr, const std::vector<std::string> &sv = {})
        : Identifier(t, n), type(tr), special_values(sv)
    { return; }

    std::string print() const;

    TypeRef& get_typeref();
    bool has_sv() const;
    std::vector<std::string>& get_sv();

    void push_depends(std::vector<TypeTag> &, const std::vector<TypeTag> &);
};

// ======================================================================================================
// Union/Structure

class BaseStruct : public Identifier
{
protected:
    std::vector<Field> fields;

public:
    BaseStruct()
        : Identifier()
    { return; }

    BaseStruct(const std::string &t, const std::string &n, const std::vector<Field> &f = {})
        : Identifier(t, n), fields(f)
    { return; }

    BaseStruct(const BaseStruct &bs)
        : Identifier(bs.text, bs.name), fields(bs.fields)
    { return; }

    std::string print_delim(char) const;

    void push_field(const Field &);
    std::vector<Field> get_fields() const;

    void push_depends(std::vector<TypeTag> &, const std::vector<TypeTag> &);
};

/*
unionname "[" "\n"
	(fieldname type "\n")+
"]" ("[" attribute* "]")?
*/
class Union : public BaseStruct
{
public:
    Union()
        : BaseStruct()
    { return; }

    Union(const std::string &t, const std::string &n, const std::vector<Field> &f = {})
        : BaseStruct(t, n, f)
    { return; }

    Union(const BaseStruct &bs)
        : BaseStruct(bs)
    { return; }

    std::string print() const;
};

/*
structname "{" "\n"
	(fieldname type ("(" fieldattribute* ")")? "\n")+
"}" ("[" attribute* "]")?
*/
class Structure : public BaseStruct
{
public:
    Structure()
        : BaseStruct()
    { return; }

    Structure(const std::string &t, const std::string &n, const std::vector<Field> &f = {})
        : BaseStruct(t, n, f)
    { return; }

    Structure(const BaseStruct &bs)
        : BaseStruct(bs)
    { return; }

    std::string print() const;

    int find_outoverlay() const;
};

// ======================================================================================================
// Flag

/*
flagname = const ["," const]*
flagname = "\"" literal "\"" ["," "\"" literal "\""]*
*/
class Flag : public Identifier
{
private:
    std::vector<std::string> values;

public:
    Flag()
        : Identifier()
    { return; }

    Flag(const std::string &t, const std::string &n, const std::vector<std::string> &v = {})
        : Identifier(t, n), values(v)
    { return; }

    std::string print() const;

    void push_depends(std::vector<TypeTag> &, const std::vector<TypeTag> &);
};

// ======================================================================================================
// Syscall

// syscallname "(" [arg ["," arg]*] ")" [type] ["(" attribute* ")"]
class Syscall : public Identifier
{
private:
    std::vector<Field> args;
    std::string return_type;

    std::vector<TypeTag> resources_used;
    std::vector<TypeTag> resources_produced;
    bool checked_used;
    bool checked_produced;

public:
    Syscall()
        : Identifier(), checked_used(false), checked_produced(false)
    { return; }

    Syscall(const std::string &t, const std::string &n, const std::vector<Field> &a = {}, const std::string &rt = "")
        : Identifier(t, n), args(a), return_type(rt), checked_used(false), checked_produced(false)
    { return; }

    std::string print() const;

    void push_field(const Field &);
    void set_return(const std::string &);
    bool has_return() const;

    void push_depends(std::vector<TypeTag> &, const std::vector<TypeTag> &);

    std::vector<TypeTag> get_resources_used(const std::vector<TypeTag> &,
                        const std::vector<TypeOneline> &, const std::vector<TypeMultiline> &,
                        const std::vector<Union> &, const std::vector<Structure> &);
    std::vector<TypeTag> get_resources_produced(const std::vector<TypeTag> &,
                        const std::vector<TypeOneline> &, const std::vector<TypeMultiline> &,
                        const std::vector<Union> &, const std::vector<Structure> &);

    int total_resources(const std::vector<TypeTag> &, const std::vector<TypeOneline> &,
                            const std::vector<TypeMultiline> &, const std::vector<Union> &,
                            const std::vector<Structure> &);
};

// ======================================================================================================
// Useful funcs

void item_push_sorted(std::vector<TypeTag> &, const TypeTag &);
int find_in_items(const std::vector<TypeTag> &, const TypeTag &);
std::vector<TypeTag> find_in_items(const std::vector<TypeTag> &, const std::string &);
bool is_in_items(const std::vector<TypeTag> &, const TypeTag &);
bool is_in_needed(const std::vector<TypeTag> &, const TypeTag &);
bool is_in_includes(const std::vector<Include> &, const std::string &);
bool is_in_typeref(const std::vector<TypeRef> &, const std::string &);
bool is_in_string(const std::vector<std::string> &, const std::string &);
int find_in_syscalls(const std::vector<Syscall> &, const std::string &);
int stoi_custom(const std::string &);

#endif
