#ifndef UNTITLED_BEAMSTATEUTIL_H
#define UNTITLED_BEAMSTATEUTIL_H

#include "Command/Command.h"
#include "StateHash.h"

inline void apply_cmd_to_hash(command cmd, StateHash &sh) {
    sh.do_cmd(cmd);
}

inline void apply_rotations_to_hash(command cmd, int count, StateHash &sh) {
    for (int i = 0; i < count; i++) {
        apply_cmd_to_hash(cmd, sh);
    }
}

#endif //UNTITLED_BEAMSTATEUTIL_H

