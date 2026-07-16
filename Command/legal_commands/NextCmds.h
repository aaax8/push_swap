//
//

#ifndef UNTITLED_NEXTCMDS1SIDE_H
#define UNTITLED_NEXTCMDS_H

#include "../Command.h"
#include "../IsCommand.h"
#include "../InitCommand.h"


extern array<array<vector<command>, NO_COMMAND + 1>, NO_COMMAND + 1> next_cmds;


extern array<array<array<bool, NO_COMMAND + 1>, NO_COMMAND + 1>, NO_COMMAND + 1> can_next_cmds;

extern vector<command> get_nexts(command last_a, command last_b);



#endif //UNTITLED_NEXTCMDS1SIDE_H
