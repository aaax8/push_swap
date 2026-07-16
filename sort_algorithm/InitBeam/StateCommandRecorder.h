#ifndef UNTITLED_STATECOMMANDRECORDER_H
#define UNTITLED_STATECOMMANDRECORDER_H

#include "all_include.h"
#include "State.h"
#include "util.h"
#include "Command/InitCommand.h"

template<int Size>
class StateCommandRecorder {
private:
    State& state;
    inline static vec_array<command, Size> cmd_history;

    static void clear_history() {
        cmd_history.clear();
    }
public:
    explicit StateCommandRecorder(State& st) : state(st) {
        my_assert(cmd_history.size()==0);
    }

    ~StateCommandRecorder() {
        my_assert(cmd_history.size()==0);
    }

    void do_cmd(command cmd) {
        state.do_cmd(cmd);
        cmd_history.emplace_back(cmd);
    }
    static const vec_array<command, Size>& get_history() {
        return cmd_history;
    }

    static size_t history_size() {
        return cmd_history.size();
    }

    void revert_all() {
        for (int i = (int)cmd_history.size() - 1; i >= 0; i--) {
            command cmd = cmd_history[i];
            state.do_cmd(revert_cmd[cmd]);
        }
        clear_history();
    }
};

template<int MAX_N>
constexpr int get_state_cmd_recorder_size() {
    return MAX_N * 4 + 10;
}

#endif //UNTITLED_STATECOMMANDRECORDER_H

