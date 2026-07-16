//
//

#ifndef UNTITLED_COMMAND_H
#define UNTITLED_COMMAND_H
#include "../all_include.h"
#include "../util.h"

enum command {
    SA = 0, SB, SS, PA, PB, RA, RB, RR, RRA, RRB, RRR, NO_COMMAND
};


extern vector<command> ALL_COMMANDS;


extern string cmd_str[];
extern const int CMD_NUM;


string i_to_cmdstr(int i);

ostream &operator<<(ostream &os, command cmd);


struct cmd_list_t {
    vector<command> list;

    bool operator==(const cmd_list_t& rhs)const{
        return list == rhs.list;
    }

    cmd_list_t() {

    }

    cmd_list_t(const vector<command>& rhs):list(rhs){

    }

    cmd_list_t(const cmd_list_t&rhs):list(rhs.list){

    }

    cmd_list_t& operator=(const cmd_list_t& rhs){
        list = rhs.list;
        return (*this);
    }

    void add(const command &cmd) {
        list.push_back(cmd);
    }

    void put_cmd() {
        for (auto &v: list) {
            out(v);
        }
    }
};

ostream &operator<<(ostream &os, const cmd_list_t &a);

void print_with_comma(const vector<command>& cmds);
#endif //UNTITLED_COMMAND_H
