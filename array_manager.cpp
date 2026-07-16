//
//

#include "array_manager.h"
#include "util.h"

constexpr int ARRAY500_NUM = 300000;
static int private_array500[ARRAY500_NUM][501];
static bool used_array[ARRAY500_NUM];
int private_array500_id = 0;

int *get_clean_array500(int size, int &array_id){
    int* arr = get_dust_array500(array_id);
    for (int i = 0; i < size; i++){
        arr[i] = 0;
    }
    return arr;
}

//確保したidを引数で受け取る
int *get_dust_array500(int &array_id){
    int loop_cnt = 0;
    while(used_array[private_array500_id]){
        if(private_array500_id + 1 == ARRAY500_NUM){
            private_array500_id = 0;
        }else{
            private_array500_id++;
        }
        loop_cnt++;
        if(loop_cnt > ARRAY500_NUM){
            out("memory error");
            my_assert(0);
        }
    }
    used_array[private_array500_id] = true;
    array_id = private_array500_id;
    return private_array500[private_array500_id];
}

//array_idのarrayをフリーにする
void free_array500(int array_id){
    my_assert(0<=array_id && array_id < ARRAY500_NUM && used_array[array_id]);
    used_array[array_id] = false;
}
