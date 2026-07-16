//
//

#include "InvNumOfPS.h"
void test_inv(){
    InvNumOfPS<100> ic;
    ic.add(10);ic.print_bit();
    ic.add(3);ic.print_bit();
    ic.add(2);ic.print_bit();
    ic.add(4);ic.print_bit();
    ic.add(5);ic.print_bit();
    ic.add(8);ic.print_bit();
    ic.add(1);ic.print_bit();
    out(ic.get_inv());
}
