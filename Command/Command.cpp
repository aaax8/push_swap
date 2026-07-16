#include "../all_include.h"
#include "../util.h"
//#include "State.h"
#include "Command.h"

//
//

vector<command> ALL_COMMANDS = {
//        SA, SB, SS, PA, PB, RA, RB, RR, RRA, RRB, RRR
        SS, RR, RRR, SA, SB, PA, PB, RA, RB, RRA, RRB
};

string cmd_str[] = {"sa", "sb", "ss", "pa", "pb", "ra", "rb", "rr", "rra", "rrb", "rrr", "NOCOM"};
string cmd_str_big[] = {"SA", "SB", "SS", "PA", "PB", "RA", "RB", "RR", "RRA", "RRB", "RRR", "NOCOM"};

const int CMD_NUM = 11;

string i_to_cmdstr(int i) {
    my_assert(0 <= i && i < NO_COMMAND + 1);
    return cmd_str[i];
}

void print_with_comma(const vector<command>& cmds){
    for (int i = 0; i < cmds.size(); i++){
        cout<< cmd_str_big[(int)cmds[i]];
        if(i+1<cmds.size()){
            cout<<", ";
        }
    }
    cout<<endl;
}

ostream &operator<<(ostream &os, command cmd) {
    os << i_to_cmdstr((int) cmd);
    return os;
}

ostream &operator<<(ostream &os, const cmd_list_t &a) {
    os << a.list;
    return os;
}

#include "Command.h"
