//
//

#ifndef UNTITLED_INITCOMMAND_H
#define UNTITLED_INITCOMMAND_H

#include "Command.h"

extern bool was_init_cmds;
extern array<command, 11> revert_cmd;

//部分的に打ち消されるコマンド
//例 SS, SA  これはSBと等価
extern array<vector<command>, 11> harf_revert;

//todo offset revertこれでやる
void init_revert_cmd();

extern array<command, 12> sync_partner;

//(i) := iを含むsyncコマンド
extern array<command, 12> sync_top;

//SSならSA
extern array<command, 12>sync_child_a;
extern array<command, 12>sync_child_b;

void init_sync_cmd();

#endif //UNTITLED_INITCOMMAND_H
