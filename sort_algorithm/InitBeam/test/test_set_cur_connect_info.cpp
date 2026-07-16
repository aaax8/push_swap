//@COMMENTED_OUT: BeamQueryからconnect_infoメンバが削除されたため、このテストは動作しなくなりました
// 今後他の処理でConnectInfoを使う場合は、BeamQueryを使わない形でテストを書き直す必要があります
/*
#include "../BeamQuery.h"
#include "../BeamSearcher.h"
#include "all_include.h"
#include "util.h"
#include "../../../../TimeKeeper.h"

//void verify_connect_info(const BeamQuery &prev_q, BeamQuery cur_q,
//                         const ConnectInfo &expected) {
//  BeamSearcher::set_cur_connect_info(prev_q, cur_q);
//
//  my_assert(cur_q.connect_info == expected);
//}

int rr_cnt(command cmd1, command cmd2, int a_cnt, int b_cnt) {
  if (cmd1 == RA && cmd2 == RB) {
    return min(a_cnt, b_cnt);
  } else {
    return 0;
  }
}

int rrr_cnt(command cmd1, command cmd2, int a_cnt, int b_cnt) {
  if (cmd1 == RRA && cmd2 == RRB) {
    return min(a_cnt, b_cnt);
  } else {
    return 0;
  }
}

int rem_ra(command cmd1, command cmd2, int a_cnt, int b_cnt) {
  if (cmd1 == RA) {
    return a_cnt - rr_cnt(cmd1, cmd2, a_cnt, b_cnt);
  } else {
    return 0;
  }
}

int rem_rb(command cmd1, command cmd2, int a_cnt, int b_cnt) {
  if (cmd2 == RB) {
    return b_cnt - rr_cnt(cmd1, cmd2, a_cnt, b_cnt);
  } else {
    return 0;
  }
}

int rem_rra(command cmd1, command cmd2, int a_cnt, int b_cnt) {
  if (cmd1 == RRA) {
    return a_cnt - rrr_cnt(cmd1, cmd2, a_cnt, b_cnt);
  } else {
    return 0;
  }
}

int rem_rrb(command cmd1, command cmd2, int a_cnt, int b_cnt) {
  if (cmd2 == RRB) {
    return b_cnt - rrr_cnt(cmd1, cmd2, a_cnt, b_cnt);
  } else {
    return 0;
  }
}

BeamQuery build_q(command cmd1, command cmd2, int a_cnt, int b_cnt) {
  return BeamQuery(99999, BeamSearcher::calc_ins_turn(cmd1, cmd2, a_cnt, b_cnt),
                   a_cnt, b_cnt, cmd1, cmd2);
}

struct ConnectTestInput {
  bool prev_has_sa;
  command prev_a_cmd;
  command prev_b_cmd;
  command cur_a_cmd;
  command cur_b_cmd;
  int prev_a_cnt;
  int prev_b_cnt;
  int cur_a_cnt;
  int cur_b_cnt;
};

struct ConnectTestParams {
  int pre_a_cmd_diff;
  int pre_b_cmd_diff;
  int cur_a_cmd_diff;
  int cur_b_cmd_diff;
  bool pre_make_sb;
  bool cur_make_sa;
  int pre_diff_turn;
  int cur_diff_turn;
};

struct RotateCounts {
  int rr;
  int rrr;
  int raw_ra;
  int raw_rb;
  int raw_rra;
  int raw_rrb;
  int rem_ra;
  int rem_rb;
  int rem_rra;
  int rem_rrb;

  RotateCounts(command cmd1, command cmd2, int a_cnt, int b_cnt)
      : rr(::rr_cnt(cmd1, cmd2, a_cnt, b_cnt)),
        rrr(::rrr_cnt(cmd1, cmd2, a_cnt, b_cnt)),
        rem_ra(::rem_ra(cmd1, cmd2, a_cnt, b_cnt)),
        rem_rb(::rem_rb(cmd1, cmd2, a_cnt, b_cnt)),
        rem_rra(::rem_rra(cmd1, cmd2, a_cnt, b_cnt)),
        rem_rrb(::rem_rrb(cmd1, cmd2, a_cnt, b_cnt)) {
    if (cmd1 == RA) {
      raw_ra = a_cnt;
      raw_rra = 0;
    } else if (cmd1 == RRA) {
      raw_rra = a_cnt;
      ;
      raw_ra = 0;
    } else {
      my_assert(0);
    }
    if (cmd2 == RB) {
      raw_rb = b_cnt;
      raw_rrb = 0;
    } else if (cmd2 == RRB) {
      raw_rrb = b_cnt;
      raw_rb = 0;
    } else {
      my_assert(0);
    }
  }
};

bool test_connect_with_params(const ConnectTestInput &input,
                              const ConnectTestParams &params) {
  int bef_pre_ins_turn =
      BeamSearcher::calc_ins_turn(input.prev_a_cmd, input.prev_b_cmd,
                                  input.prev_a_cnt, input.prev_b_cnt) +
      input.prev_has_sa;
  int bef_cur_ins_turn = BeamSearcher::calc_ins_turn(
      input.cur_a_cmd, input.cur_b_cmd, input.cur_a_cnt, input.cur_b_cnt);

  BeamQuery prev_q = build_q(input.prev_a_cmd, input.prev_b_cmd,
                             input.prev_a_cnt, input.prev_b_cnt);
  // テストを簡単にするために、beam_queryとconnect_infoの中身を同じにしてるが、厳密にはおかしい
  // prev_has_saを作っているのに、中身が同じになっていることがある、テスト上は問題ないと判断
  prev_q.connect_info =
      ConnectInfo(input.prev_has_sa, false テスト外, 0 テスト外,
                  input.prev_a_cnt, input.prev_b_cnt, bef_pre_ins_turn);
  BeamQuery cur_q = build_q(input.cur_a_cmd, input.cur_b_cmd, input.cur_a_cnt,
                            input.cur_b_cnt);
  BeamSearcher::set_cur_connect_info(prev_q, cur_q);
  ConnectInfo expected =
      ConnectInfo(params.cur_make_sa, params.pre_make_sb, params.pre_diff_turn,
                  input.cur_a_cnt + params.cur_a_cmd_diff,
                  input.cur_b_cnt + params.cur_b_cmd_diff,
                  bef_cur_ins_turn + params.cur_diff_turn);
  if (cur_q.connect_info != expected) {
    BeamSearcher::set_cur_connect_info(prev_q, cur_q);
    my_assert(false);
    return false;
  }
  return true;
}

ConnectTestParams BuildConnectTestParams(bool connect_a, bool connect_b,
                                         int pre_diff_turn, int cur_diff_turn) {
  ConnectTestParams params;
  if (connect_a) {
    params.pre_a_cmd_diff = -1;
    params.cur_a_cmd_diff = -1;
    params.cur_make_sa = true;
  } else {
    params.pre_a_cmd_diff = 0;
    params.cur_a_cmd_diff = 0;
    params.cur_make_sa = false;
  }
  if (connect_b) {
    params.pre_b_cmd_diff = -1;
    params.cur_b_cmd_diff = -1;
    params.pre_make_sb = true;
  } else {
    params.pre_b_cmd_diff = 0;
    params.cur_b_cmd_diff = 0;
    params.pre_make_sb = false;
  }
  params.pre_diff_turn = pre_diff_turn;
  ;
  params.cur_diff_turn = cur_diff_turn;
  ;
  return params;
}

bool test_connect_pre_ra_rb(const ConnectTestInput &input,
                            const RotateCounts &pre_counts,
                            const RotateCounts &cur_counts) {
  // a:  ra, rb |  ra, rrb
  // rraが無い、 rrbがある
  if (cur_counts.raw_rra == 0 && cur_counts.raw_rrb > 0) {
    // a1 : rrはあるがrbが余っていないので、左の数が減らない
    if (pre_counts.rr > 0 && pre_counts.rem_ra >= 0 && pre_counts.rem_rb == 0 &&
        cur_counts.rem_ra >= 0 && cur_counts.rem_rrb > 0) {
      //-rb, +ra, +sb  | -rrb
      if (input.prev_has_sa && pre_counts.rr == 1) {
        // ssが作れる
        return test_connect_with_params(
            input, BuildConnectTestParams(false, true, 0, -1));
      } else {
        return test_connect_with_params(
            input, BuildConnectTestParams(false, true, 1, -1));
      }
      // a2 : rrがありrbが余っているので、左の数が減る
    } else if (pre_counts.rr > 0 && pre_counts.rem_ra == 0 &&
               pre_counts.rem_rb > 0 && cur_counts.rem_ra >= 0 &&
               cur_counts.rem_rrb > 0) {
      // rrが消えないので、ssはつくっても手数が減らない
      //-rb, +ra, +sb  | -rrb
      return test_connect_with_params(
          input, BuildConnectTestParams(false, true, 0, -1));
      // b : ra, rb | rra, rb
      // b1 : ra, rraが余っている
    } else {
      return test_connect_with_params(
          input, BuildConnectTestParams(false, false, 0, 0));
    }
    //b
  } else if (cur_counts.raw_rra > 0 && cur_counts.raw_rrb == 0) {
    if (pre_counts.rr > 0 && pre_counts.rem_ra > 0 && pre_counts.rem_rb == 0 &&
        cur_counts.rem_rra > 0 && cur_counts.rem_rb >= 0) {
      //-ra | +sa, -rra
      return test_connect_with_params(
          input, BuildConnectTestParams(true, false, -1, 0));
      // b2 : ra余ってない, rra余ってる
    } else if (pre_counts.rr > 0 && pre_counts.rem_ra == 0 &&
               pre_counts.rem_rb >= 0 && cur_counts.rem_rra > 0 &&
               cur_counts.rem_rb >= 0) {
      //-ra(rbの方が長い) |  +sa, -rra
      return test_connect_with_params(
          input, BuildConnectTestParams(true, false, 0, 0));

    } else {
      return test_connect_with_params(
          input, BuildConnectTestParams(false, false, 0, 0));
    }
    // c1 :
  } else if (cur_counts.raw_rra > 0 && cur_counts.raw_rrb > 0) {
    if (input.prev_has_sa && pre_counts.rr == 1) {
      return test_connect_with_params(
          input, BuildConnectTestParams(true, true, -1, 0));
    } else {

      return test_connect_with_params(input,
                                      BuildConnectTestParams(true, true, 0, 0));
    }
  } else {
    // なにも起きない
    return test_connect_with_params(input,
                                    BuildConnectTestParams(false, false, 0, 0));
  }
}

bool test_connect_pre_ra_rrb(const ConnectTestInput &input,
                             const RotateCounts &cur_counts) {
  // d
  if (cur_counts.raw_rra > 0 && cur_counts.raw_rrb == 0) {
    return test_connect_with_params(input,
                                    BuildConnectTestParams(true, false, -1, 0));
    // e
  } else if (cur_counts.raw_rra > 0 && cur_counts.raw_rrb > 0) {
    // e1
    if (cur_counts.rrr > 0 && cur_counts.rem_rra > 0 &&
        cur_counts.rem_rrb == 0) {
      return test_connect_with_params(
          input, BuildConnectTestParams(true, false, -1, 0));
      // e2
    } else if (cur_counts.rrr > 0 && cur_counts.rem_rra == 0 &&
               cur_counts.rem_rrb >= 0) {
      return test_connect_with_params(
          input, BuildConnectTestParams(true, false, -1, 1));
    } else {
      return test_connect_with_params(
          input, BuildConnectTestParams(false, false, 0, 0));
    }
  } else {
    return test_connect_with_params(input,
                                    BuildConnectTestParams(false, false, 0, 0));
  }
}

bool test_connect_pre_rra_rb(const ConnectTestInput &input,
                             const RotateCounts &cur_counts) {
  // f
  if (cur_counts.raw_rra == 0 && cur_counts.raw_rrb > 0) {
    if (input.prev_has_sa) {
      return test_connect_with_params(
          input, BuildConnectTestParams(false, true, -1, -1));
    } else {
      return test_connect_with_params(
          input, BuildConnectTestParams(false, true, 0, -1));
    }
    // g
  } else if (cur_counts.raw_rra > 0 && cur_counts.raw_rrb > 0) {
    // g1
    if (cur_counts.rrr > 0 && cur_counts.rem_rra >= 0 &&
        cur_counts.rem_rrb == 0) {
      if (input.prev_has_sa) {
        return test_connect_with_params(
            input, BuildConnectTestParams(false, true, -1, 0));
      } else {
        return test_connect_with_params(
            input, BuildConnectTestParams(false, true, 0, 0));
      }
      // g2
    } else if (cur_counts.rrr > 0 && cur_counts.rem_rra == 0 &&
               cur_counts.rem_rrb > 0) {
      if (input.prev_has_sa) {
        return test_connect_with_params(
            input, BuildConnectTestParams(false, true, -1, -1));
      } else {
        return test_connect_with_params(
            input, BuildConnectTestParams(false, true, 0, -1));
      }
    } else {
      return test_connect_with_params(
          input, BuildConnectTestParams(false, false, 0, 0));
    }
  } else {
    return test_connect_with_params(input,
                                    BuildConnectTestParams(false, false, 0, 0));
  }
}

bool test_connect(const ConnectTestInput &input, const RotateCounts &pre_counts,
                  const RotateCounts &cur_counts) {
  // a, b, c(rrが無い場合は考えなくていい, たとえばpreの場合、ra, rb(0)は、
  // ra, rrb(0)として処理する方が都合がいい)
  if (pre_counts.raw_ra > 0 && pre_counts.raw_rb > 0) {
    return test_connect_pre_ra_rb(input, pre_counts, cur_counts);
    // d, e
  } else if (pre_counts.raw_ra > 0 && pre_counts.raw_rb == 0) {
    return test_connect_pre_ra_rrb(input, cur_counts);
    // f, g
  } else if (pre_counts.raw_ra == 0 && pre_counts.raw_rb > 0) {
    return test_connect_pre_rra_rb(input, cur_counts);
  } else {
    // 何も短縮が起きない
    return test_connect_with_params(input,
                                    BuildConnectTestParams(false, false, 0, 0));
  }
}

void test_all() {
  vector<command> rotate_a_cmds = {RA, RRA};
  vector<command> rotate_b_cmds = {RB, RRB};
  for (int prev_has_sa = 0; prev_has_sa < 2; prev_has_sa++) {
    for (auto prev_a_cmd : rotate_a_cmds) {
      for (auto prev_b_cmd : rotate_b_cmds) {
        for (auto cur_a_cmd : rotate_a_cmds) {
          for (auto cur_b_cmd : rotate_b_cmds) {
            for (int prev_a_cnt = 0; prev_a_cnt < 5; prev_a_cnt++) {
              for (int prev_b_cnt = 0; prev_b_cnt < 5; prev_b_cnt++) {
                for (int cur_a_cnt = 0; cur_a_cnt < 5; cur_a_cnt++) {
                  for (int cur_b_cnt = 0; cur_b_cnt < 5; cur_b_cnt++) {
                    ConnectTestInput input;
                    input.prev_has_sa = prev_has_sa;
                    input.prev_a_cmd = prev_a_cmd;
                    input.prev_b_cmd = prev_b_cmd;
                    input.cur_a_cmd = cur_a_cmd;
                    input.cur_b_cmd = cur_b_cmd;
                    input.prev_a_cnt = prev_a_cnt;
                    input.prev_b_cnt = prev_b_cnt;
                    input.cur_a_cnt = cur_a_cnt;
                    input.cur_b_cnt = cur_b_cnt;

                    RotateCounts pre_counts(input.prev_a_cmd, input.prev_b_cmd,
                                            input.prev_a_cnt, input.prev_b_cnt);
                    RotateCounts cur_counts(input.cur_a_cmd, input.cur_b_cmd,
                                            input.cur_a_cnt, input.cur_b_cnt);
                    my_assert(test_connect(input, pre_counts, cur_counts));
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

void test_set_cur_connect_info() {
  auto start_time = get_time_sec();
  out("========================================");
  out("Testing set_cur_connect_info");
  out("========================================");
  test_all();
  auto end_time = get_time_sec();
  out("========================================");
  out("All tests passed!");
  out("========================================");
  out("Test execution time:", end_time - start_time, "seconds");
}
*/
