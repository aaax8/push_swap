//
//

#ifndef UNTITLED_INVNUMOFPS_H
#define UNTITLED_INVNUMOFPS_H

#include "all_include.h"
#include "util.h"
#include "array_manager.h"
#include "debug.h"

template<size_t ARG_N>
class InvNumOfPS {
    bitset<ARG_N> exist;
    int inv_cnt=0;

public:
    void print_bit(){
        for (int i = 0; i < ARG_N; i++){
            if(exist[i]){
                cout<<i<<", ";
            }
        }
        cout<<endl;
    }

    InvNumOfPS(){

    }

    //距離が近い場合はswapの距離でやることにして
    //そうでない場合は、pbしてからpaする分+2する
    void add(int v){
        my_assert(0<=v && v < ARG_N);
        bitset<ARG_N> tmp = exist>>v;
        int pop_cnt = tmp.count();
        if(pop_cnt < 3){
            inv_cnt += pop_cnt;
        }else{
            inv_cnt += pop_cnt + 2;
        }
        exist[v] = true;
    }

    int get_inv(){
        return inv_cnt;
    }
};

void test_inv();
#endif //UNTITLED_INVNUMOFPS_H
