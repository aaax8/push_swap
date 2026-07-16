//
//

#ifndef UNTITLED_RINGBUFFER500_H
#define UNTITLED_RINGBUFFER500_H

#include "all_include.h"
#include "util.h"
#include "array_manager.h"
#include "debug.h"

constexpr int MAX_BUFF = 501;//push_backしてからpop_frontしたりするので1大きい必要ある
class RingBuffer500{
private:
    //arrayにおけるi番目のidxを返す
    int get_idx_of_arr(int i){
        my_assert(0<=i && i<sz);
        int idx = s + i;
        if(idx >= MAX_BUFF){
            return idx - MAX_BUFF;
        }else{
            return idx;
        }
    }
public:

    int s, t, sz;
    int *array;
    int array_id;
#ifdef DEBUG
    deque<int> debug_dq;//速度を頑張る時は消す
#endif

    bool operator==(const RingBuffer500& rhs)const{
        if(sz != rhs.sz){
            return false;
        }
        for (int i = 0; i < sz; i++){
            if(operator[](i) != rhs[i]){
                return false;
            }
        }
        return true ;
    }

    void chk_dq_sz()const{
#ifdef DEBUG
        my_assert(sz == debug_dq.size());
#endif
    }

    int size()const {
        chk_dq_sz();
        return sz;
    }

    void print()const{
        stringstream ss;
        for (int i = 0; i < sz; i++){
            if (i > 0) {
                ss << ", ";
            }
            ss << operator[](i);
        }
        out(ss.str());
    }


    RingBuffer500& operator=(const RingBuffer500& rhs){
#ifdef DEBUG
        debug_dq.clear();
#endif
        s = rhs.s;
        t = rhs.s;
        sz = 0;
        for (int i = 0; i < rhs.sz; i++){
            push_back(rhs[i]);
        }
        return *this;
    }

    template<class T>
    RingBuffer500(initializer_list<T> list): s(0), t(0), sz(0){
        array = get_dust_array500(array_id);
        auto it = list.begin();
        while(it != list.end()){
            push_back(*it);
            it++;
        }
    }

    RingBuffer500(const RingBuffer500& rhs):s(rhs.s), t(rhs.s), sz(0){
        array = get_dust_array500(array_id);
        for (int i = 0; i < rhs.sz; i++){
            push_back(rhs[i]);
        }
    }

    RingBuffer500(): s(0), t(0), sz(0){
        array = get_dust_array500(array_id);
    }

    RingBuffer500(const deque<int>& dq): s(0), t(0), sz(0){
        array = get_dust_array500(array_id);
        for(auto& v : dq){
            push_back(v);
        }
    }

    bool empty() const {
        return size()==0;
    }

    int front(){
        return array[s];
    }

    int back(){
        if(t==0){
            return array[MAX_BUFF-1];
        }else{
            return array[t-1];
        }
    }

    void push_back(int rhs){
#ifdef DEBUG
        debug_dq.push_back(rhs);
#endif
        sz++;
        my_assert(sz <= MAX_BUFF);
        array[t] = rhs;
        if(t == MAX_BUFF - 1){
            t = 0;
        }else{
            t++;
        }
        chk_dq_sz();
    }

    void pop_back(){
#ifdef DEBUG
        debug_dq.pop_back();
#endif
        my_assert(sz > 0);
        sz--;
        if(t==0){
            t = MAX_BUFF-1;
        }else{
            t--;
        }
        chk_dq_sz();
    }

    void push_front(int val){
#ifdef DEBUG
        debug_dq.push_front(val);
#endif
        sz++;
        my_assert(sz <= MAX_BUFF);
        if(s==0){
            s = MAX_BUFF-1;
        }else{
            s--;
        }
        array[s] = val;
        chk_dq_sz();
    }

    void pop_front(){
#ifdef DEBUG
        debug_dq.pop_front();
#endif
        my_assert(sz > 0);
        sz--;
        if(s==MAX_BUFF-1){
            s = 0;
        }else{
            s++;
        }
        chk_dq_sz();
    }

    void swap_top() {
#ifdef DEBUG
        swap(debug_dq[0], debug_dq[1]);
#endif
        swap(array[get_idx_of_arr(0)], array[get_idx_of_arr(1)]);
        chk_dq_sz();
    }

    int& operator[](int i){
        my_assert(0<=i && i<sz);
        return array[get_idx_of_arr(i)];
    }

    const int& operator[](int i)const {
        my_assert(0<=i && i<sz);
        int idx = s + i;
        if(idx >= MAX_BUFF){
            return array[idx - MAX_BUFF];
        }else{
            return array[idx];
        }
    }

    ~RingBuffer500(){
        free_array500(array_id);
    }

    int min_i(){
        int N = size();
        int min_v = numeric_limits<int>::max();
        int res_i=-1;
        for (int i = 0; i < N; i++){
            if(chmi(min_v, operator[](i))){
                res_i = i;
            }
        }
        my_assert(0 <= res_i && res_i < N);
        return res_i;
    }

    int min_v(){
        return operator[](min_i());
    }

    int max_i(){
        int N = size();
        int max_v = -(1<<30);
        int res_i=-1;
        for (int i = 0; i < N; i++){
            if(chma(max_v, operator[](i))){
                res_i = i;
            }
        }
        return res_i;
    }

    int max_v(){
        return operator[](max_i());
    }
};

inline ostream& operator<<(ostream& os, const RingBuffer500& buf) {
    for (int i = 0; i < buf.size(); i++) {
        if (i > 0) {
            os << ", ";
        }
        os << buf[i];
    }
    return os;
}

deque<int> to_dq(RingBuffer500& buf);

void test_ring_buffer500();
void ring_buf_eq();
//!
int get_pos(const RingBuffer500& buf, int tar);

#endif //UNTITLED_RINGBUFFER500_H
