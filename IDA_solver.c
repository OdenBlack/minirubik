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

/* Orientation 與 Permutation PDB */
static uint8_t o_pdb[O_STATES];
static uint8_t p_pdb[P_STATES];

/* 記錄當前解題路徑 (步數編碼) */
static uint8_t solution_path[MAX_DEPTH];

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
/*                                              
static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};
*/
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

/* ida_dfs() 呼叫次數以及搜尋期間的 quarter_turn() 呼叫次數*/
static uint64_t ida_dfs_calltime = 0;
static uint64_t quarter_turn_calltime = 0;

/* 轉動1/4圈 */
static state_t quarter_turn(state_t state, uint8_t face)
{
    ++quarter_turn_calltime;
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
 
/* 對 permutation 以及 orientation 取 heuristic 的最大值 */
static inline uint8_t heuristic(const state_t *s){
    uint8_t h_p = p_pdb[rank_permutation(s)];
    uint8_t h_o = o_pdb[rank_orientation(s)];
    return (h_o > h_p) ? h_o : h_p;
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

// 進行 IDA* 的 DFS
static int ida_dfs(state_t s, int g, int bound, int last_f, int *next_bound) {
    uint8_t h = heuristic(&s);
    int f = g + h;
    ++ida_dfs_calltime;

    /* 剪枝：預估步數超出目前門檻 */
    if (f > bound) {
        if (f < *next_bound)
            *next_bound = f;
        return -1;
    }

    /* 達到終點狀態 */
    if (is_solved(&s)) 
        return g;

    for (int face = 0; face < 3; ++face) {
        if (face == last_f)
            continue; /* 動作剪枝：避免對同一面連續轉動 */

        state_t next = s;
        for (int quarter = 1; quarter <= 3; ++quarter) {
            next = quarter_turn(next, face);

            /* 記錄目前步驟編碼: face * 3 + (quarter - 1) */
            solution_path[g] = (uint8_t) (face * 3 + (quarter - 1));
            /* 成功找到解答，若 ret >= 0 則將實際步數沿著 Call Stack 網上傳*/
            int ret = ida_dfs(next, g + 1, bound, face, next_bound);
            if (ret > 0)
                return ret;
        }
    }
    return -1;
}

/* 求解函式 */
static int solve_state(state_t start) {
    ida_dfs_calltime = 0;               // 重設 ida_dfs_calltime 計數器
    quarter_turn_calltime = 0;          // 重設 quarter_turn_calltime 計數器
    if (is_solved(&start))
        return 0;
    int bound = heuristic(&start);

    while (bound <= MAX_DEPTH) {
        int next_bound = 999;
        
        int dis = ida_dfs(start, 0, bound, -1, &next_bound);
        if (dis > 0) {
            /* 依序印出每一步動作 (原 minirubik 輸出格式) */
            for (int i = 0; i < dis; ++i) {
                uint8_t move = solution_path[i];
                printf("%s%s", move_names[move], (i == dis - 1) ? "" : " "); // 避免印出多於空格
            }
            putchar('\n');
            return dis;
        }

        if (next_bound == 999)
            break;

        bound = next_bound; /* 擴展門檻，進入下一輪搜尋 */
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
    fprintf(stderr, "Call times of quarter_turn: %" PRIu64 "\n", quarter_turn_calltime);    

    /* 原版 solved state 會輸出空行。 */
    if (solution_length == 0)
        putchar('\n');

    return 0;
}