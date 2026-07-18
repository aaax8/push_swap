//
//

#include "OptimizeRangeSearch.h"

namespace {
    // human_review_begin: push_swap CLI では command 以外を stdout に出さないようにするため。
    bool optimize_range_log_enabled = true;
    // human_review_end
}

// human_review_begin: push_swap CLI では command 以外を stdout に出さないようにするため。
// 責務: optimize_all_range の進捗ログ出力可否を切り替える。
// 必要な理由: tester 実行時だけ l=... の stdout 出力を止めるため。
void set_optimize_range_log_enabled(bool enabled) {
    optimize_range_log_enabled = enabled;
}
// human_review_end

void test_shortest_path_holder(){
    //短い
    {
        State start({1,2,3,4}, {5,6,7,8});
        State goal(start);
        goal.ra();
        goal.pb();
        goal.rra();
        ShortestPathHolder sph(start, goal, goal.turn() - start.turn());
        sph.try_upd_path({SA}, {PA});
        out("has_ans", sph.has_answer());
        out("best_path_holder", sph.get_shortest_path());
    }
    {
        State start({1,2,3,4, 9, 10, 11}, {5,6,7,8});
        State goal(start);
        goal.sa();goal.ra();
        goal.sa();goal.ra();
        goal.sa();goal.ra();
        ShortestPathHolder sph(start, goal, goal.turn() - start.turn());
        //PB, RA, RA, RA, PA
        sph.try_upd_path({PB, RA}, {PB, RRA, RRA});
        out("has_ans", sph.has_answer());
        out("best_path_holder", sph.get_shortest_path());
    }
}

void optimize_all_range(State& st, int range, int left){
    for (size_t l = left; l < st.turn(); l++)
    {
        // human_review_begin: 通常実験では進捗ログを保ち、push_swap CLI では stdout 汚染を止めるため。
        if (optimize_range_log_enabled) {
            //todo
            cerr << "l=" << l << endl;
        }
        // human_review_end
        int r = l + range;
        if (r > st.turn()) {
            break;
        }
        LegalStateCmd leg;
        OptimizeRangeSearch<LegalStateCmd> opt(st, leg, l, r);
        ShortestPathHolder best_path = opt.optimize_range();
        if(!best_path.has_answer()){
            continue;
        }
        st.set_equiv_cmds(l, r, best_path.get_shortest_path());
    }
}
