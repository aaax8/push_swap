//
// Created by Auto
//

#ifndef UNTITLED_BEAMOPERATIONS_H
#define UNTITLED_BEAMOPERATIONS_H

#include "all_include.h"
#include "RingBuffer500.h"

class BeamOperations {
public:
    static void ra(RingBuffer500& A) {
        if (A.size() >= 2) {
            A.push_back(A[0]);
            A.pop_front();
        }
    }

    static void rb(RingBuffer500& B) {
        if (B.size() >= 2) {
            B.push_back(B[0]);
            B.pop_front();
        }
    }

    static void rr(RingBuffer500& A, RingBuffer500& B) {
        ra(A);
        rb(B);
    }

    static void rra(RingBuffer500& A) {
        if (A.size() >= 2) {
            A.push_front(A.back());
            A.pop_back();
        }
    }

    static void rrb(RingBuffer500& B) {
        if (B.size() >= 2) {
            B.push_front(B.back());
            B.pop_back();
        }
    }

    static void rrr(RingBuffer500& A, RingBuffer500& B) {
        rra(A);
        rrb(B);
    }

    static void sa(RingBuffer500& A) {
        if (A.size() >= 2) {
            A.swap_top();
        }
    }

    static void sb(RingBuffer500& B) {
        if (B.size() >= 2) {
            B.swap_top();
        }
    }

    static void ss(RingBuffer500& A, RingBuffer500& B) {
        sa(A);
        sb(B);
    }

    static void pa(RingBuffer500& B, RingBuffer500& A) {
        if (B.size() >= 1) {
            A.push_front(B[0]);
            B.pop_front();
        }
    }

    static void pb(RingBuffer500& A, RingBuffer500& B) {
        if (A.size() >= 1) {
            B.push_front(A[0]);
            A.pop_front();
        }
    }

    static void apply_command(RingBuffer500& A, RingBuffer500& B, command cmd) {
        switch (cmd) {
            case SA:
                sa(A);
                break;
            case SB:
                sb(B);
                break;
            case SS:
                ss(A, B);
                break;
            case PA:
                pa(B, A);
                break;
            case PB:
                pb(A, B);
                break;
            case RA:
                ra(A);
                break;
            case RB:
                rb(B);
                break;
            case RR:
                rr(A, B);
                break;
            case RRA:
                rra(A);
                break;
            case RRB:
                rrb(B);
                break;
            case RRR:
                rrr(A, B);
                break;
            default:
                break;
        }
    }
};

#endif //UNTITLED_BEAMOPERATIONS_H

