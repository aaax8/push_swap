//
//

#include "SyncCommand.h"
bool can_sync_cmd(const command &cmd1, const command &cmd2) {
    if (cmd1 == SA && cmd2 == SB) {
        return true;
    } else if (cmd1 == SB && cmd2 == SA) {
        return true;
    } else if (cmd1 == RA && cmd2 == RB) {
        return true;
    } else if (cmd1 == RB && cmd2 == RA) {
        return true;
    } else if (cmd1 == RRA && cmd2 == RRB) {
        return true;
    } else if (cmd1 == RRB && cmd2 == RRA) {
        return true;
    } else {
        return false;
    }
}

command sync_cmd(const command &cmd1, const command &cmd2) {
    if (cmd1 == SA && cmd2 == SB) {
        return SS;
    } else if (cmd1 == SB && cmd2 == SA) {
        return SS;
    } else if (cmd1 == RA && cmd2 == RB) {
        return RR;
    } else if (cmd1 == RB && cmd2 == RA) {
        return RR;
    } else if (cmd1 == RRA && cmd2 == RRB) {
        return RRR;
    } else if (cmd1 == RRB && cmd2 == RRA) {
        return RRR;
    } else {
        my_assert(false);
        return PA;
    }
}
