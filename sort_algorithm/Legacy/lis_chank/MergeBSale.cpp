//
//

#include "MergeBSale.h"
//todo サイズ適当
//500の時とかまずい
//array<array<pair<int,int>, 20 >, (1<<MAX_CHANK_SZ)> dp;
pair<int, int> g_dp[(1<<MAX_CHANK_SZ)][20];

//第二引数はchankのvalのid, 返り値は実際の値
int g_nex_a[1<<MAX_CHANK_SZ][MAX_CHANK_SZ];
int g_new_nex_a[1<<MAX_CHANK_SZ][MAX_CHANK_SZ];

//第二引数は実際の値, 返り値はソートしたときに何番目か(差分を見る)
int g_a_ord[1<<MAX_CHANK_SZ][500];
