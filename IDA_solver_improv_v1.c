#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

enum {
    C = 7,
    O_STATES = 729,     /* 3^6 */
    P_STATES = 5040,    /* 7! */
    MAX_DEPTH = 11,      /* 二階魔術方塊在HTM下的最大可能最佳步數 */
    MOVES = 9           /* 一次總共九種轉動方式 */
};

/* 魔術方塊狀態 */
typedef struct {
    uint8_t p[C], o[C];
} state_t;

/* 額外定義新的, 用 rank 表示的狀態 */
typedef struct {
    uint16_t p_rank;
    uint16_t o_rank;
} coord_t;

/* Orientation 與 Permutation PDB */
static uint8_t o_pdb[O_STATES];
static uint8_t p_pdb[P_STATES];

/* 記錄當前解題路徑 (步數編碼) */
static uint8_t solution_path[MAX_DEPTH];

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};

/* 三種面的轉向變化 */
static const uint8_t source[3][C] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][C] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

/* 額外建立 transition table*/
static uint16_t p_transition[3][P_STATES];
static uint16_t o_transition[3][O_STATES];
static int transitions_ready;
static uint64_t transition_pair_calls = 0;

/* ida_dfs() 呼叫次數 */
static uint64_t ida_dfs_calltime = 0;
// static uint64_t quarter_turn_calltime = 0;

/* 轉動1/4圈 */
static state_t quarter_turn(state_t state, uint8_t face)
{
    // ++quarter_turn_calltime;
    state_t result;
    for (uint8_t i = 0; i < C; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

/* 將 7 個角塊的 5040 種排列用 Lehmer code 做 rank */
static inline int rank_permutation(const state_t *state){
    uint32_t p = 0;
    for (uint8_t i = 0; i < C; ++i) {
        uint8_t smaller = 0;
        for (uint8_t j = (uint8_t) (i + 1U); j < C; ++j)
            if (state->p[j] < state->p[i])
                ++smaller;
        p = p * (C - i) + smaller;
    }
    return p;
}

/* 將前 6 個角塊 729 個朝向進做 rank */
static inline int rank_orientation(const state_t *state){
    int rank = 0;
    for(int i = 0; i < 6; i++){
        rank = rank * 3 + state->o[i];
    }
    return rank;
}

/* 用現有的 quarter_turn() 建表；搜尋本身之後改用 rank 表。 */
static void unrank_permutation(uint16_t rank, state_t *state)
{
    uint8_t available[C];
    unsigned factorial = 720;

    for (unsigned i = 0; i < C; ++i)
        available[i] = (uint8_t)i;

    for (unsigned i = 0; i < C; ++i) {
        unsigned digit = rank / factorial;
        rank = (uint16_t)(rank % factorial);
        state->p[i] = available[digit];

        for (unsigned j = digit; j + 1U < C - i; ++j)
            available[j] = available[j + 1U];

        if (i + 1U < C)
            factorial /= C - i - 1U;
    }

    for (unsigned i = 0; i < C; ++i)
        state->o[i] = 0;
}

static void unrank_orientation(uint16_t rank, state_t *state)
{
    unsigned sum = 0;

    for (unsigned i = 0; i < C; ++i)
        state->p[i] = (uint8_t)i;

    for (int i = C - 2; i >= 0; --i) {
        state->o[i] = (uint8_t)(rank % 3U);
        sum += state->o[i];
        rank = (uint16_t)(rank / 3U);
    }

    state->o[C - 1] = (uint8_t)((3U - sum % 3U) % 3U);
}

/*
* unrank_permutation(r) 先把 rank 還原成角塊排列
* unrank_orientation(r) 先把 rank 還原成前六個朝向值
* 再依照「七個朝向總和模 3 為 0」補出第七個朝向
* 轉動後再用 rank_orientation() 與 rank_permutation() 算出轉動後將會抵達哪個 rank
* 最後放入 transition table 中
*/
static void init_transition_tables(void)
{
    for (uint8_t face = 0; face < 3; ++face) {
        for (uint16_t rank = 0; rank < P_STATES; ++rank) {
            state_t state = {0};
            unrank_permutation(rank, &state);
            state = quarter_turn(state, face);
            p_transition[face][rank] =
                (uint16_t)rank_permutation(&state);
        }

        for (uint16_t rank = 0; rank < O_STATES; ++rank) {
            state_t state = {0};
            unrank_orientation(rank, &state);
            state = quarter_turn(state, face);
            o_transition[face][rank] =
                (uint16_t)rank_orientation(&state);
        }
    }

    transitions_ready = 1;
}

/* 對 permutation 以及 orientation 取 heuristic 的最大值 */
static inline uint8_t heuristic(const state_t *s){
    uint8_t h_p = p_pdb[rank_permutation(s)];
    uint8_t h_o = o_pdb[rank_orientation(s)];
    return (h_o > h_p) ? h_o : h_p;
}

/* 基於 coord_t 計算 heuristic*/
static inline uint8_t coord_heuristic(coord_t state)
{
    uint8_t hp = p_pdb[state.p_rank];
    uint8_t ho = o_pdb[state.o_rank];
    return (hp > ho) ? hp : ho;
}

/* 對 p_pdb 透過 BFS 預先建表 */
static void init_p_pdb(void){
    memset(p_pdb, 0xFF, sizeof(p_pdb));     // 初始化未走訪標記(0xFF)
    state_t q[P_STATES];                    // BFS 佇列 BFS 佇列 (只是host建表時的暫存 queue, 大小為 14*5040 = 70,560 bytes, 不會算入靜態資料大小)
    int head = 0, tail = 0;

    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    // 呼叫 rank_permutation(&solved) 會將全 0 的朝向編碼為索引 0
    // 將 p_pdb[0] = 0, 代表已經轉正的狀態, 距離轉正的最少步數為 0 步
    p_pdb[rank_permutation(&solved)] = 0;   
    q[tail++] = solved;                     //將 solved 推入 BFS queue, 作為探索的 root       


    // 從轉正的狀態作為起點，透過 BFS 向外展開
    while(head < tail){
        state_t cur = q[head++];                        // 目前探索的節點
        uint8_t d = p_pdb[rank_permutation(&cur)];      // 透過目前節點的 rank 進行查表，確認最短步數

        // 對當前的節點的三個面嘗試轉動
        for(int f = 0; f < 3; ++f){
            state_t next = cur;
            // 每個面都要轉三次
            for(int k = 1; k <= 3; ++k){
                next = quarter_turn(next, f);
                int r =  rank_permutation(&next);

                // 查表，如果還未拜訪過，就丟進 BFS queue 中
                if(p_pdb[r] == 0xFF){
                    p_pdb[r] = d + 1;                   // 記錄最短步數
                    q[tail++] = next;                   // 丟進 BFS queue 中
                }
            }
        }
    }
}

/* 對 o_pdb 透過 BFS 預先建表 */
static void init_o_pdb(void){
    memset(o_pdb, 0xFF, sizeof(o_pdb));      // 初始化未走訪標記(0xFF)
    state_t q[O_STATES];                    // BFS 佇列 (只是host建表時的暫存 queue, 大小為 14*729 = 10,206 bytes , 不會算入靜態資料大小)
    int head = 0, tail = 0;

    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    // 呼叫 rank_orientation(&solved) 會將全 0 的朝向編碼為索引 0
    // 將 o_pdb[0] = 0, 代表已經轉正的狀態, 距離轉正的最少步數為 0 步
    o_pdb[rank_orientation(&solved)] = 0;   
    q[tail++] = solved;                     //將 solved 推入 BFS queue, 作為探索的 rott       

    // 從轉正的狀態作為起點，透過 BFS 向外展開
    while(head < tail){
        state_t cur = q[head++];                        // 目前探索的節點
        uint8_t d = o_pdb[rank_orientation(&cur)];      // 透過目前節點的 rank 進行查表，確認最短步數

        // 對當前的節點的三個面嘗試轉動
        for(int f = 0; f < 3; ++f){
            state_t next = cur;
            // 每個面都要轉三次
            for(int k = 1; k <= 3; ++k){
                next = quarter_turn(next, f);
                int r =  rank_orientation(&next);

                // 查表，如果還未拜訪過，就丟進 BFS queue 中
                if(o_pdb[r] == 0xFF){
                    o_pdb[r] = d + 1;                   // 記錄最短步數
                    q[tail++] = next;                   // 丟進 BFS queue 中
                }
            }
        }
    }
}

/* 判斷方塊狀態是否合法 */
static int parse_state(const char *text, state_t *state) {
    if (!text || !state)
        return 0;

    if (strlen(text) != C * 2)
        return 0;

    unsigned seen = 0;
    unsigned o_sum = 0;

    for (int i = 0; i < C; ++i) {
        unsigned p = (unsigned)(text[i] - '1');
        unsigned o = (unsigned)(text[i + C] - '1');

        /* 數值範圍與 Bitmap 重複角塊檢查 */
        if (p >= C || o >= 3 || ((seen >> p) & 1U))
            return 0;

        state->p[i] = (uint8_t)p;
        state->o[i] = (uint8_t)o;
        seen |= (1U << p);
        o_sum += o;
    }

    /* 物理守恆檢查：活動角塊朝向總和模 3 必須合法 */
    if (o_sum % 3 != 0)
        return 0;

    return 1;
}

/* 輸出解析或求解失敗訊息 
static void output_failed(void) {
    puts("error");
}
*/

/*  需要判斷方塊是否還原 */
static inline int is_solved(const state_t *s) {
    for (int i = 0; i < C; ++i) {
        if (s->p[i] != i || s->o[i] != 0)               // 確認 state 的 permutation 是依照 0~6, orientation 都全為 0
            return 0;
    }
    return 1;
}

/* 基於 coord_t 的 ida_dfs */
static int ida_dfs(coord_t state, int g, int bound, int last_face,
                   int *next_bound)
{
    int f = g + coord_heuristic(state);
    ++ida_dfs_calltime;

    if (f > bound) {
        if (f < *next_bound)
            *next_bound = f;
        return -1;
    }

    if (state.p_rank == 0 && state.o_rank == 0)
        return g;

    for (uint8_t face = 0; face < 3; ++face) {
        if (face == last_face)
            continue;

        coord_t next = state;

        /* Repeated quarter-turn lookups produce the three HTM moves. */
        for (uint8_t quarter = 1; quarter <= 3; ++quarter) {
            next.p_rank = p_transition[face][next.p_rank];
            next.o_rank = o_transition[face][next.o_rank];
            ++transition_pair_calls;

            solution_path[g] =
                (uint8_t)(face * 3U + (quarter - 1U));

            int ret = ida_dfs(next, g + 1, bound, face, next_bound);
            if (ret > 0)
                return ret;
        }
    }

    return -1;
}

/* 基於 coord_t 的求解函式 */
static int solve_state(state_t start)
{
    if (!transitions_ready)
        init_transition_tables();

    ida_dfs_calltime = 0;
    transition_pair_calls = 0;

    if (is_solved(&start))
        return 0;

    coord_t initial = {
        (uint16_t)rank_permutation(&start),
        (uint16_t)rank_orientation(&start)
    };

    int bound = coord_heuristic(initial);

    while (bound <= MAX_DEPTH) {
        int next_bound = 999;
        int dis = ida_dfs(initial, 0, bound, -1, &next_bound);

        if (dis > 0) {
            for (int i = 0; i < dis; ++i) {
                uint8_t move = solution_path[i];
                printf("%s%s", move_names[move],
                       (i == dis - 1) ? "" : " ");
            }
            putchar('\n');
            return dis;
        }

        if (next_bound == 999)
            break;

        bound = next_bound;
    }

    return -1;
}

/* self test */
static int self_test(void) {
    /* 驗證已解開狀態 */
    state_t solved = {0};
    for (int i = 0; i < C; ++i)
        solved.p[i] = (uint8_t)i;

    if (!is_solved(&solved))
        return 0;

    if (heuristic(&solved) != 0)
        return 0;

    /* 驗證一步旋轉後能正確求得 1 步解 */
    state_t step1 = quarter_turn(solved, 0);
    int res = solve_state(step1);
    if (res != 1)
        return 0;

    return 1;
}

int main(int argc, char **argv)
{
    const char *program =
        (argc > 0 && argv[0]) ? argv[0] : "IDA_solver";

    if (argc == 2 && strcmp(argv[1], "--self-test") == 0) {
        init_o_pdb();
        init_p_pdb();

        if (!self_test()) {
            fprintf(stderr, "self-test failed\n");
            return 1;
        }

        puts("Self test passed.");
        return 0;
    }

    state_t state;
    if (argc != 2 || !parse_state(argv[1], &state)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n", program);
        return 2;
    }

    init_o_pdb();
    init_p_pdb();

    int solution_length = solve_state(state);
    
    if (solution_length < 0) {
        fprintf(stderr, "could not solve state\n");
        return 1;
    }
    fprintf(stderr, "Call times of ida_dfs: %" PRIu64 "\n", ida_dfs_calltime);              // PRIu64 是 <inttypes.h> 提供的格式字串巨集，用來搭配 printf/fprintf 輸出 uint64_t
    fprintf(stderr, "Call times of transition pair calls: %" PRIu64 "\n", transition_pair_calls);    

    /* 原版 solved state 會輸出空行。 */
    if (solution_length == 0)
        putchar('\n');

    return 0;
}