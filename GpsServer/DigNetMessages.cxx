/*
vim: ts=4
vim: shiftwidth=4
*/

#include "DigNetMessages.h"
#include <utils/mystring.h>

using namespace utils;
using namespace std;

/*****************************************************************************/
void
ParseDigNetMessage(
        std::string&                        name,
        std::map<std::string, std::string>& args,
        const std::string&                  msg
)
{
    name.clear();
    args.clear();

    // 1. Split off the first one - name.
    std::string  s;
    {
        unsigned int space_pos = 0;
        unsigned int nonspace_pos = 0;
        for (unsigned int i = 0; i < msg.size(); ++i) {
            if (msg[i] == ' ') {
                space_pos = i;
                break;
            }
        }
        for (unsigned int i = space_pos + 1; i < msg.size(); ++i) {
            if (msg[i] != ' ') {
                nonspace_pos = i;
                break;
            }
        }

        if (space_pos > 0 && nonspace_pos > space_pos) {
            name = msg.substr(0, space_pos);
            s = msg.substr(nonspace_pos);
        } else {
            s = msg;
        }
    }

    // 2. Parse arguments.
    std::string val;
    for (unsigned int so_far = 0; so_far < s.size();) {
        const std::string::size_type    eqpos = s.find('=', so_far);
        if (eqpos == std::string::npos) {
            // insert empty one.
            const std::string key(strip(s.substr(so_far, s.size() - so_far)));
            if (key.size() != 0) {
                args.insert(pair<string, string>(key, ""));
            }
            break;
        }
        const std::string::size_type    startpos = eqpos + 1;
        const std::string       key(s.substr(so_far, eqpos - so_far));
        std::string::size_type      next_so_far = startpos;
        val.resize(0);
        if (s.size() > startpos) {
            if (s[startpos] == '"') {
                const std::string::size_type    endpos = s.find('"', startpos + 1);
                if (endpos == std::string::npos) {
                    // error.
                    throw Error("Error: unmatched \" for key %s in record %s", key.c_str(), s.c_str());
                } else {
                    val = s.substr(startpos + 1, endpos - startpos - 1);
                    next_so_far = endpos + 1;
                }
            } else {
                const std::string::size_type    endpos = s.find(' ', startpos);
                if (endpos == std::string::npos) {
                    val = s.substr(startpos);
                    next_so_far = s.size() - 1;
                } else {
                    val = s.substr(startpos, endpos - startpos);
                    next_so_far = endpos + 1;
                }
            }
        }
        args.insert(pair<string, string>(key, val));

        while (next_so_far < s.size() && s[next_so_far] == ' ') {
            ++next_so_far;
        }
        so_far = next_so_far;
    }
}

/*****************************************************************************/
void
ParseDigNetMessage(
        std::string&                        name,
        std::map<std::string, std::string>& args,
        const utils::CMR&                   Packet
)
{
    std::string msg(Packet.data.begin(), Packet.data.end());
    ParseDigNetMessage(name, args, msg);
}

/*****************************************************************************/
bool
GetArg(
        const std::map<std::string, std::string>&   args,
        const std::string&                          key,
        std::string&                                value)
{
    map<string, string>::const_iterator elem = args.find(key);
    const bool  found = elem != args.end();
    if (found) {
        value = elem->second;
    }
    return found;
}

/*****************************************************************************/
bool
GetArg(
        const std::map<std::string, std::string>&   args,
        const std::string&                          key,
        double&                                     value)
{
    map<string, string>::const_iterator elem = args.find(key);
    const bool  ok = elem != args.end() && double_of(elem->second, value);
    return ok;
}

/*****************************************************************************/
bool
GetArg(
        const std::map<std::string, std::string>&   args,
        const std::string&                          key,
        int&                                        value)
{
    map<string, string>::const_iterator elem = args.find(key);
    const bool  ok = elem != args.end() && int_of(elem->second, value);
    return ok;
}

/*****************************************************************************/
bool
GetArg(
        const std::map<std::string, std::string>&   args,
        const std::string&                          key,
        double&                                     value1,
        double&                                     value2
)
{
    map<string, string>::const_iterator elem = args.find(key);
    const std::string::size_type        semi_pos =
        elem == args.end()
        ? std::string::npos
        : elem->second.find(';');
    if (semi_pos != std::string::npos) {
        double  v1;
        double  v2;
        const std::string   s1(elem->second.substr(0, semi_pos));
        const std::string   s2(elem->second.substr(semi_pos + 1));
        const bool ok = double_of(s1, v1) && double_of(s2, v2);
        if (ok) {
            value1 = v1;
            value2 = v2;
        }
        return ok;
    }
    return false; // failure.
}

/*****************************************************************************/
bool
GetArg(
        const std::map<std::string, std::string>&   args,
        const std::string&                          key,
        std::vector<double>&                        value
)
{
    map<string, string>::const_iterator elem = args.find(key);
    if (elem == args.end()) {
        return false;
    } else {
        vector<string>  v;
        split(elem->second, ";", v);
        vector<double>  vf;
        for (unsigned int i = 0; i < v.size(); ++i) {
            double  f;
            if (double_of(v[i], f)) {
                value.push_back(f);
            }
        }
        return true;
    }
}

/*****************************************************************************/
bool
GetArg(
        const std::map<std::string, std::string>&   args,
        const std::string&                          key,
        utils::my_time&                             value)
{
    map<string, string>::const_iterator elem = args.find(key);
    const bool  ok = elem != args.end() && my_time_of_string(elem->second, value);
    return ok;
}

/*****************************************************************************/
bool
GetArg(
        const std::map<std::string, std::string>&   args,
        WorkArea&                                   workArea)
{
    WorkArea    wa;
    bool        ok = true;
    ok = ok && GetArg(args, "Change_Time", wa.Change_Time);
    ok = ok && GetArg(args, "Start", wa.Start_X, wa.Start_Y);
    ok = ok && GetArg(args, "End", wa.End_X, wa.End_Y);
    ok = ok && GetArg(args, "Step_Forward", wa.Step_Forward);
    ok = ok && GetArg(args, "Pos_Sideways", wa.Pos_Sideways);
    if (ok) {
        workArea = wa;
    }
    return ok;
}
