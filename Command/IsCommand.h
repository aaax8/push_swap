//
//

#ifndef UNTITLED_ISCOMMAND_H
#define UNTITLED_ISCOMMAND_H

#include "Command.h"

bool is_a_cmd(const command &cmd);

bool is_b_cmd(const command &cmd);

bool is_sync_cmd(const command &cmd);

bool is_push_cmd(const command &cmd);

bool is_sync_push_cmd(const command &cmd);

bool is_offset(command &cmd1, command &cmd2);

#endif //UNTITLED_ISCOMMAND_H
