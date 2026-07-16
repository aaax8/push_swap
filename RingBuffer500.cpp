//
//

#include "RingBuffer500.h"

deque<int> to_dq(RingBuffer500& buf){
    deque<int> dq;
    for (int i = 0; i < buf.size(); i++){
        dq.push_back(buf[i]);
    }
    return dq;
}
void test_ring_buffer500(){
//    {
//        RingBuffer500 rb2 = {1,2,3,10};
//        deque<int> q= to_dq(rb2);
//        out(q);
//        out(rb2.front(), rb2.back());
//        return;
//    }
    RingBuffer500 rb;
    for (int i = 0; i < 500; i++){
        rb.push_back(i);
    }
    for (int i = 0; i < 250; i++){
        rb.pop_front();
    }
    for (int i = 0; i < 250; i++){
        rb.push_front(i);
    }
    rb.print();
    out("-----------");
    for (int i = 0; i < 500; i++){
        rb.pop_back();
    }
        rb.push_back(1);rb.print();
    rb.push_back(3);rb.print();
    rb.push_back(4);rb.print();
    rb.push_front(5);rb.print();
    rb.push_front(7);rb.print();
    rb.pop_front();rb.print();
    rb.pop_front();rb.print();
    rb.pop_back();rb.print();
    rb.pop_back();rb.print();
    rb.push_front(10);rb.print();
    rb.pop_back();rb.print();

}

void ring_buf_eq(){
    RingBuffer500 a;
    for (int i = 0; i < 500; i++){
        a.push_back(i);
    }
    for (int i = 0; i < 500; i++){
        a.pop_front();
    }
    for (int i = 0; i < 50; i++){
        a.push_back(i);
    }
    RingBuffer500 b;
    for (int i = 0; i < 50; i++){
        b.push_back(i);
    }
    my_assert(a == b);
    out("ok");
}

int get_pos(const RingBuffer500& buf, int tar) {
    for (int i = 0; i < buf.size(); i++) {
        if (buf[i] == tar) {
            return i;
        }
    }
    return -1;
}
