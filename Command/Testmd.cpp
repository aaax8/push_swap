//
//

#include "Testmd.h"

command rand_cmd(){
    return (command)(rand()%11);
}

command rand_a_cmd(){
    command cmd = rand_cmd();
    while(!is_a_cmd(cmd)){
        cmd = rand_cmd();
    }
    return cmd;
}

command rand_b_cmd(){
    command cmd = rand_cmd();
    while(!is_b_cmd(cmd)){
        cmd = rand_cmd();
    }
    return cmd;
}

command rand_ab_cmd(){
    command cmd = rand_cmd();
    while(!is_a_cmd(cmd) && !is_b_cmd(cmd)){
        cmd = rand_cmd();
    }
    return cmd;
}
command rand_sync_cmd(){
    command cmd = rand_cmd();
    while(!is_sync_cmd(cmd)){
        cmd = rand_cmd();
    }
    return cmd;
}


command rand_push_cmd(){
    command cmd = rand_cmd();
    while(!is_push_cmd(cmd)){
        cmd = rand_cmd();
    }
    return cmd;
}


command rand_push_sync_cmd(){
    command cmd = rand_cmd();
    while(!is_sync_push_cmd(cmd)){
        cmd = rand_cmd();
    }
    return cmd;
}
