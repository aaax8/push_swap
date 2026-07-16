//
//

#ifndef UNTITLED_SYNCCOMMAND_H
#define UNTITLED_SYNCCOMMAND_H

#include "Command.h"

bool can_sync_cmd(const command &cmd1, const command &cmd2);
command sync_cmd(const command &cmd1, const command &cmd2);

#endif //UNTITLED_SYNCCOMMAND_H
