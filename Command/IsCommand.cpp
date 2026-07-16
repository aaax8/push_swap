//
//

#include "IsCommand.h"

bool is_a_cmd(const command &cmd) {
    return (cmd == SA ||
            cmd == RA ||
            cmd == RRA);
}

bool is_b_cmd(const command &cmd) {
    return (cmd == SB ||
            cmd == RB ||
            cmd == RRB);
}

bool is_sync_cmd(const command &cmd) {
    return (cmd == SS ||
            cmd == RR ||
            cmd == RRR);
}

bool is_push_cmd(const command &cmd) {
    return (cmd == PA ||
            cmd == PB);
}

bool is_sync_push_cmd(const command &cmd) {
    return is_sync_cmd(cmd) || is_push_cmd(cmd);
}


bool is_offset(command &cmd1, command &cmd2) {
//            SA, SB, SS, PA, PB, RA, RB, RR, RRA, RRB, RRR
    if (cmd1 == SA && cmd2 == SA) {
        return true;
    } else if (cmd1 == SB && cmd2 == SB) {
        return true;
    } else if (cmd1 == SS && cmd2 == SS) {
        return true;
    } else if (cmd1 == PA && cmd2 == PB) {
        return true;
    } else if (cmd1 == PB && cmd2 == PA) {
        return true;
    } else if (cmd1 == RA && cmd2 == RRA) {
        return true;
    } else if (cmd1 == RB && cmd2 == RRB) {
        return true;
    } else if (cmd1 == RR && cmd2 == RRR) {
        return true;
    } else if (cmd1 == RRA && cmd2 == RA) {
        return true;
    } else if (cmd1 == RRB && cmd2 == RB) {
        return true;
    } else if (cmd1 == RRR && cmd2 == RR) {
        return true;
    } else {
        return false;
    }
}
