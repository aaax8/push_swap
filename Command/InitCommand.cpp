#include "InitCommand.h"
bool was_init_cmds = false;
array<command, 11> revert_cmd;
array<vector<command>, 11> harf_revert;
void init_revert_cmd() {
    revert_cmd[SA] = SA;
    revert_cmd[SB] = SB;
    revert_cmd[SS] = SS;
    revert_cmd[PA] = PB;
    revert_cmd[PB] = PA;
    revert_cmd[RA] = RRA;
    revert_cmd[RB] = RRB;
    revert_cmd[RR] = RRR;
    revert_cmd[RRA] = RA;
    revert_cmd[RRB] = RB;
    revert_cmd[RRR] = RR;


    harf_revert[SA] = {SS};
    harf_revert[SB] = {SS};
    harf_revert[SS] = {SA, SB};
//    harf_revert[PA];
//    harf_revert[PB];
    harf_revert[RA] = {RRR};
    harf_revert[RB] = {RRR};
    harf_revert[RR] = {RRA, RRB};
    harf_revert[RRA] = {RR};
    harf_revert[RRB] = {RR};
    harf_revert[RRR] = {RA, RB};
}

array<command, 12> sync_partner;

//(i) := iを含むsyncコマンド
array<command, 12> sync_top;

//SSならSA
array<command, 12>sync_child_a;
array<command, 12>sync_child_b;


void init_sync_cmd() {
    for (int i = 0; i < 12; i++) {
        sync_partner[i] = NO_COMMAND;
        sync_top[i] = NO_COMMAND;
        sync_child_a[i] = NO_COMMAND;
        sync_child_b[i] = NO_COMMAND ;
    }
    sync_partner[SA] = SB;
    sync_partner[SB] = SA;
    sync_partner[RA] = RB;
    sync_partner[RB] = RA;
    sync_partner[RRA] = RRB;
    sync_partner[RRB] = RRA;

    sync_top[SA] = SS;
    sync_top[SB] = SS;
    sync_top[RA] = RR;
    sync_top[RB] = RR;
    sync_top[RRA] = RRR;
    sync_top[RRB] = RRR;

    sync_child_a[SS] = SA;
    sync_child_a[RR] = RA;
    sync_child_a[RRR] = RRA;

    sync_child_b[SS] = SB;
    sync_child_b[RR] = RB;
    sync_child_b[RRR] = RRB;


}