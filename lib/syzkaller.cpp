#include <exec_api.h>
#include <file_api.h>
#include <json.h>
#include <my_string.h>
#include <syzkaller.h>

#include <iostream>
#include <string>
#include <vector>

ProgOpts::ProgOpts()
{ reset(); }

ProgOpts::ProgOpts(const std::string &progfile)
{
    reset();
    from_prog(progfile);
}

// Reset all options to their defaults
void ProgOpts::reset()
{
    threaded = true;
    repeat = 0;
    procs = 6;
    slowdown = 1;
    sandbox = "";
    sandbox_arg = 0;
    segv = false;
    tmpdir = false;

    tun = false;
    net_dev = false;
    net_reset = false;
    cgroups = false;
    binfmt_misc = false;
    close_fds = false;
    devlink_pci = false;
    nic_vf = false;
    usb = false;
    vhci = false;
    wifi = false;
    ieee802154 = false;
    sysctl = false;
    swap = false;

    collide = false;
    fault = false;
    fault_call = -1;
    fault_nth = -1;
}

std::string opts_from_syz_repro(const std::string &reprolog)
{
    std::vector<std::string> lines;
    if (!load_file(reprolog, lines))
    {
        std::cerr << "Failed to load prog file " << reprolog << std::endl << std::flush;
        return "";
    }

    int lidx = 0;
    for (lidx = 0; lidx < lines.size() && !starts_with(lines.at(lidx), "Final opts: "); lidx++);
    if (lidx >= lines.size())
        return "";
    
    return lines.at(lidx).substr(12);
}

// This function expects output from a modified Syzkaller. It will not work in general.
int ProgOpts::from_syz_repro(const std::string &reprolog)
{
    std::string opts_string = opts_from_syz_repro(reprolog);
    reset();

    JSON json;
    if (!json.parse(std::vector<std::string>({opts_string})))
        return -1;

    return from_json(json);
}

// Read prog options from syz prog file
int ProgOpts::from_prog(const std::string &progfile)
{
    std::vector<std::string> proglines;
    if (!load_file(progfile, proglines))
    {
        std::cerr << "Failed to load prog file " << progfile << std::endl << std::flush;
        return -1;
    }

    int lidx = 0;
    for (lidx = 0; lidx < proglines.size() && !starts_with(proglines.at(lidx), "#{"); lidx++);
    if (lidx >= proglines.size())
        return 0;

    std::string rawopts = proglines.at(lidx).substr(1);
    reset();

    JSON json;
    if (!json.parse(std::vector<std::string>({rawopts})))
        return -1;

    return from_json(json);
}

// Check the additional features to see if the format is for enable or disable
// return true for enable, false for disable
bool ProgOpts::enable_format(const JSON &json)
{
    std::vector<std::string> feat_strs = {"tun", "netdev", "resetnet", "cgroups", "binfmt_misc", "close_fds",
                                        "devlink_pci", "nic_vf", "usb", "vhci", "wifi", "ieee802154", "sysctl", "swap"};

    for (std::string feat : feat_strs)
        if (json.has_name(feat) && json.is_type(feat, JSON_Val_bool))
            return json.get_bool(feat);
        
    return true;
}

int ProgOpts::from_json(const JSON &json)
{
    if (json.has_name("threaded") && json.is_type("threaded", JSON_Val_bool))
        threaded = json.get_bool("threaded");
    
    if (json.has_name("repeat") && json.is_type("repeat", JSON_Val_int))
        repeat = json.get_int("repeat");
    else if (json.has_name("repeat") && json.is_type("repeat", JSON_Val_bool))
        repeat = json.get_bool("repeat") ? 0 : 1;
    
    if (json.has_name("procs") && json.is_type("procs", JSON_Val_int))
        procs = json.get_int("procs");
    
    if (json.has_name("slowdown") && json.is_type("slowdown", JSON_Val_int))
        slowdown = json.get_int("slowdown");
    
    if (json.has_name("sandbox") && json.is_type("sandbox", JSON_Val_string))
        sandbox = json.get_string("sandbox");
    
    if (json.has_name("sandbox_arg") && json.is_type("sandbox_arg", JSON_Val_int))
        sandbox_arg = json.get_int("sandbox_arg");
    
    if (json.has_name("segv") && json.is_type("segv", JSON_Val_bool))
        segv = json.get_bool("segv");
    
    if (json.has_name("tmpdir") && json.is_type("tmpdir", JSON_Val_bool))
        tmpdir = json.get_bool("tmpdir");
    
    // addtl. features
    bool doing_enable = !enable_format(json);
    if (json.has_name("tun") && json.is_type("tun", JSON_Val_bool))
        tun = json.get_bool("tun");
    else
        tun = doing_enable;

    if (json.has_name("netdev") && json.is_type("netdev", JSON_Val_bool))
        net_dev = json.get_bool("netdev");
    else
        net_dev = doing_enable;
    
    if (json.has_name("resetnet") && json.is_type("resetnet", JSON_Val_bool))
        net_reset = json.get_bool("resetnet");
    else
        net_reset = doing_enable;
    
    if (json.has_name("cgroups") && json.is_type("cgroups", JSON_Val_bool))
        cgroups = json.get_bool("cgroups");
    else
        cgroups = doing_enable;
    
    if (json.has_name("binfmt_misc") && json.is_type("binfmt_misc", JSON_Val_bool))
        binfmt_misc = json.get_bool("binfmt_misc");
    else
        binfmt_misc = doing_enable;
    
    if (json.has_name("close_fds") && json.is_type("close_fds", JSON_Val_bool))
        close_fds = json.get_bool("close_fds");
    else
        close_fds = doing_enable;
    
    if (json.has_name("devlink_pci") && json.is_type("devlink_pci", JSON_Val_bool))
        devlink_pci = json.get_bool("devlink_pci");
    else
        devlink_pci = doing_enable;
    
    if (json.has_name("nic_vf") && json.is_type("nic_vf", JSON_Val_bool))
        nic_vf = json.get_bool("nic_vf");
    else
        nic_vf = doing_enable;
    
    if (json.has_name("usb") && json.is_type("usb", JSON_Val_bool))
        usb = json.get_bool("usb");
    else
        usb = doing_enable;
    
    if (json.has_name("vhci") && json.is_type("vhci", JSON_Val_bool))
        vhci = json.get_bool("vhci");
    else
        vhci = doing_enable;
    
    if (json.has_name("wifi") && json.is_type("wifi", JSON_Val_bool))
        wifi = json.get_bool("wifi");
    else
        wifi = doing_enable;
    
    if (json.has_name("ieee802154") && json.is_type("ieee802154", JSON_Val_bool))
        ieee802154 = json.get_bool("ieee802154");
    else
        ieee802154 = doing_enable;
    
    if (json.has_name("sysctl") && json.is_type("sysctl", JSON_Val_bool))
        sysctl = json.get_bool("sysctl");
    else
        sysctl = doing_enable;
    
    if (json.has_name("swap") && json.is_type("swap", JSON_Val_bool))
        swap = json.get_bool("swap");
    else
        swap = doing_enable;
    
    // legacy features:
    if (json.has_name("collide") && json.is_type("collide", JSON_Val_bool))
        collide = json.get_bool("collide");

    if (json.has_name("fault") && json.is_type("fault", JSON_Val_bool))
        fault = json.get_bool("fault");
    
    if (json.has_name("fault_call") && json.is_type("fault_call", JSON_Val_int))
        fault_call = json.get_int("fault_call");
    
    if (json.has_name("fault_nth") && json.is_type("fault_nth", JSON_Val_int))
        fault_nth = json.get_int("fault_nth");

    return 0;
}

// Enable all additional options
void ProgOpts::enable_all()
{
    tun = true;
    net_dev = true;
    net_reset = true;
    cgroups = true;
    binfmt_misc = true;
    close_fds = true;
    devlink_pci = true;
    nic_vf = true;
    usb = true;
    vhci = true;
    wifi = true;
    ieee802154 = true;
    sysctl = true;
    swap = true;
}

// Fix the options so they don't collide with each other and dependencies are met.
// e.g. cgroups requires tmpdir
void ProgOpts::sanitize_opts()
{
    if (net_reset && repeat > 0)
        net_reset = false;
    if (tun && sandbox.empty())
        sandbox = "none";
    if (cgroups && !tmpdir)
        tmpdir = true;
}

bool ProgOpts::any_enabled() const
{
    return tun || net_dev || net_reset || cgroups || binfmt_misc || close_fds
           || devlink_pci || nic_vf || usb || vhci || wifi || ieee802154 || sysctl
           || swap;
}

bool ProgOpts::all_enabled() const
{
    return tun && net_dev && net_reset && cgroups && binfmt_misc && close_fds
           && devlink_pci && nic_vf && usb && vhci && wifi && ieee802154 && sysctl
           && swap;
}

std::string ProgOpts::enable_string() const
{
    std::string enable = "-enable=";
    enable += tun ? "tun," : "";
    enable += net_dev ? "net_dev," : "";
    enable += net_reset ? "net_reset," : "";
    enable += cgroups ? "cgroups," : "";
    enable += binfmt_misc ? "binfmt_misc," : "";
    enable += close_fds ? "close_fds," : "";
    enable += devlink_pci ? "devlink_pci," : "";
    enable += nic_vf ? "nic_vf," : "";
    enable += usb ? "usb," : "";
    enable += vhci ? "vhci," : "";
    enable += wifi ? "wifi," : "";
    enable += ieee802154 ? "ieee802154," : "";
    enable += sysctl ? "sysctl," : "";
    enable += swap ? "swap," : "";
    enable = enable.substr(0, enable.size()-1);
    return enable;
}

void ProgOpts::compile_execopts(std::vector<std::string> &optv) const
{
    if (threaded)
        optv.push_back("-threaded");
    
    optv.push_back("-repeat="+std::to_string(repeat));
    optv.push_back("-procs="+std::to_string(procs));

    if (slowdown != 1)
        optv.push_back("-slowdown="+std::to_string(slowdown));

    //if (segv)
        //optv.push_back("-segv");

    if (all_enabled())
    {
        optv.push_back("-enable=all");
        optv.push_back("-sandbox="+(sandbox.empty() ? "none" : sandbox));
        //optv.push_back("-tmpdir");
        return;
    }

    if (!sandbox.empty())
    {
        optv.push_back("-sandbox="+sandbox);
        if (sandbox_arg != 0)
            optv.push_back("-sandbox_arg="+std::to_string(sandbox_arg));
    }

    //if (tmpdir)
        //optv.push_back("-tmpdir");
    
    if (any_enabled())
        optv.push_back(enable_string());
}

std::string ProgOpts::execopts_string() const
{
    std::string ret = "";
    std::vector<std::string> opts;
    compile_execopts(opts);

    for (std::string o : opts)
        if (!o.empty())
            ret += o + " ";
    
    return ret;
}
