//
//

#ifndef UNTITLED_ENUMS_H
#define UNTITLED_ENUMS_H
enum class e_push_type {
    a2b, b2a, a2a, e_push_type_default
};
enum e_apply_mode {
    do_, revert
};
//対象の前から後ろにinsertしたか
//対象の後ろから前にinsertしたか
enum e_insert_type {
    front_to_back, back_to_front
};

enum e_stack_type {
    a, b
};

#endif //UNTITLED_ENUMS_H
