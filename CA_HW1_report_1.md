---
title: Computer Architecture HW1

---

# Computer Architecture HW1

[![hackmd-github-sync-badge](https://hackmd.io/c43Q04SMTwqe-6_phvXkZw/badge)](https://hackmd.io/c43Q04SMTwqe-6_phvXkZw)

[toc]

## 0. Version and enviornment
此文章內容皆基於以下環境
```
Host:

Operating System :                  Microsoft Windows 11
Win32_Processor :                   AMD Ryzen 7 7700
C version :                         gcc 16.2.0 (x86_64-posix-seh-rev1)
fork sysprog21/minirubik SHA :      3811ad0a87bd490e45099c3cb179ec33caf46cb5

--------------------------------------

Ripes:

Ripes version :                     Continuous release
Processor :                         RV32_ISS (ISA simulator)、RV32_5S (5-stage processor)
Instruction Set :                   RV32I
```

---

## 1. Baseline of Sovler.c and metrics

### 1-1 Principle of using BFS on solving 2x2x2 cubic 

一個 2x2x2 的魔術方塊共有 8 個 blocks，而每個 block 又存在三種的 Orientation，並且整顆魔術方塊也可以不同的面朝向觀察者，又額外產生具有具有6種組合的自由度，因此魔術方塊的總排列組合相當驚人。

本文提供了一種透過 BFS 作為核心來解決 2X2X2 魔術方塊的方法，並在最後透過 RISCV 以及對原 Sovler.c 設計新的算法來降低總體記憶體開銷。

首先，可將魔術方塊分為八個編號 0~7 的固定位置，以及編號 0~7，總共八個編號的 blocks。此外，由於每個角塊都有三種各自的朝向，需要額外引入一種朝向編碼來表示角塊當前的朝向。朝向編碼的內容如下

* 1 (internal 0): solved orientation.
* 2 (internal 1): clockwise twist $(+120^\circ).$
* 3 (internal 2): counterclockwise twist $(-120^\circ).$


只有當編號 0~7 的八個 block 落在對應編號 0~7 的位置上時，以及所有 block 的朝向編碼皆為 0 時，魔術方塊才被解開。而為了讓魔術方塊不要被整顆方塊的旋轉而帶來更多組合，我們固定一面稱為 front，讓這面時刻面向觀察者，以下列出魔術方塊的展開圖
```
                         UP
                      ┌───┬───┐
                      │ 7 │ 4 │
                      ├───┼───┤
                      │ 0 │ 1 │
                      └───┴───┘

       LEFT              FRONT             RIGHT              BACK
    ┌───┬───┐         ┌───┬───┐         ┌───┬───┐         ┌───┬───┐
    │ 7 │ 0 │         │ 0 │ 1 │         │ 1 │ 4 │         │ 4 │ 7 │
    ├───┼───┤         ├───┼───┤         ├───┼───┤         ├───┼───┤
    │ 6 │ 3 │         │ 3 │ 2 │         │ 2 │ 5 │         │ 5 │ 6 │
    └───┴───┘         └───┴───┘         └───┴───┘         └───┴───┘

                        DOWN
                      ┌───┬───┐
                      │ 3 │ 2 │
                      ├───┼───┤
                      │ 6 │ 5 │
                      └───┴───┘
```
在 front 上，block 0 被規定保持不動，因此實際可改變位置的 block 實際只有編號 1~7 的 block。因此共存在 $7! \times 3^6 = 3{,}674{,}160$ 種組合 ( 因所有朝向編碼的總和須為 3 的倍數，因此 7 個 block 的朝向實際上只需要由 6 個朝向編碼所決定即可 )。而在實際操作魔術方塊上，我們規定只有三種面可以轉動，分別為 right face ( R )、back face ( B )、down face ( D )。並且每個面共有三種轉動方式，共計九種轉動方式如下
```
R, R2, R'     B, B2, B'     D, D2, D'
```

上述規定了魔術方塊的位置與朝向表示，以及轉動方式。而 Solver.c 中對 2x2x2 cubic 的解法可簡化如下:

---
1. 將每種可能狀態的魔術方塊 block 位置與朝向編碼分別編碼成 Permutation Lehmer rank ( $p$ ) 以及 Orientation rank ( $o$ )，並在最後將兩種 rank 再編成 Composite rank ( $\text{rank}$ )，使的魔術方塊的每種狀態都能被表示成一個唯一的數字。三種 rank 的計算方式如下
$$
p = \sum_{i=0}^6 c_i \times (6 - i)!, \quad c_i = \sum_{j=i+1}^6 [s.p[j] < s.p[i]] 
$$$$
o = \sum_{i=0}^5 s.o[i] \times 3^{5 - i}
$$$$
\text{rank} = p \times 729 + o
$$

2. 將每種狀態視為一個 node，而九種轉動方式視為九條 edges，因此一個節點可透過九條邊前往其他九個不同的 node。隨後建立一個 FIFO queue，裡面放入尚未被探索過的節點。
3. 使用 BFS 遍歷所有狀態。首先將 rank = 0 的狀態訂為 solved state，並放入 queue 中。queue 將 solved state 給 pop 出來並標示為 "已探索"，並沿著九條 edges 通往其他九個不同的 stste，如果抵達的狀態尚未被探索，則將該狀態加入到 queue 中，並紀錄該狀態通往 solved state 的轉動方式，持續以上動作直到所有狀態均被探索過，即可建立出一張用以解開魔術方塊的表。
4. 當表建立完成時，查詢時不必再搜尋：先把輸入狀態編成索引，查表取得下一步，轉動後再查一次，直到回到完成狀態。每走一步，距離都會減少 1，因此將會得到最短解，最遠的狀態距離是 11 步。
---

### 1-2 Invarient in slover.c

slover.c 中，魔術方塊當前的狀態被表示成如下
```clike=
typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;
```
最初始的 solved state 即為如下
```clike=
[0,1,2,3,4,5,6]
[0,0,0,0,0,0,0]
/*
雖然段落 1.1 的說明中，編號 1~7 的 blocks 是實際可動的
但是在這邊我們依然將編號從 1~7 設定回 0~6 
*/
```
接下來是有關轉動的表示，以及反向轉動的表示方法
```clike=
static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};
//inverse_move中，只需要表示成move_names的陣列中，這個轉動的反向轉動的所在編號即可 
```
再來，使用了 source 以及 twist，表示當前位置是通過哪一個位置轉動而來，以及這個位置所造成的朝向編碼變化量
```clike=
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};
static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};
```

整體程式的主要開銷都在 build_table 的部分，需要遍歷所有節點。其記憶體開銷如下
| Allocation | Size | Storage |
| :- | :- | :- |
|```toward_solved```, one move byte per state|3,674,160 B, 3.504 MiB|heap|
|```queue```, one 4-byte rank per state|14,696,640 B, 14.016 MiB|heap|
|Transition tables, $3×(5,040+729)$ ```uint16_t```|34,614 B, 33.80 KiB	automatic|in build_table|
|Computed peak|18,405,414 B, 17.553 MiB||
		
---
### 1-3 Guest performance in host

有兩種指標可以用來衡量 Ripes 在 host 上的效能，分別為 Host-bytes-per-guest-byte 以及 retired-instructions-per-second

* Host-bytes-per-guest-byte: Ripes(guest) 在主機上 (host) 為每個 guest memory byte 額外用了多少記憶體，即 host bytes ÷ guest bytes
    * 作法 : 利用 power shall 指令  ``` Get-Process -Name Ripes | Select-Object Id, PrivateMemorySize64 ``` 獲取執行前與執行後 Ripes 的 private bytes ，相減後得到不同 Guest size 之下的 Private Bytes，然後將不同 case 的 Private Bytes 與 Guest size 相除。
    * 下表為不同寫入大小所產生的 Private Bytes 以及 Host-bytes-per-guest-byte。由結果可知，隨著寫入大小的上升，比例隨測試區域變大而下降，表示每次執行都存在一部分固定開銷，因此取寫入大小為 262144 以及 65536 bytes 的結果，重新計算 Host-bytes-per-guest-byte
    
| Guest size (bytes)| pre - executed Private Bytes | post - executed Private Bytes | difference | Host-bytes-per-guest-byte (bytes/guest bytes)|
| :-: | :-: | :-: | :-: | :-: |
| 4096  | 30003200 | 30666752 | 663552 | 162 |
| 16384  | 29872128 | 31862784  | 1990656 | 121.5 | 
| 65536 | 29822976  | 35971072 | 6148096 | 93.8 |
| 262144 | 29888512  | 52359168 | 22470656 | 85.7 |

$$ R_{\text{Hby/Gby, ISS}} = \frac{ 22470656 - 6148096 }{ 262144 - 65536 } = 83 \text{  bytes/guest bytes} $$





* retired-instructions-per-second: Ripes 每秒能模擬執行多少條指令 = instructions/second
    * 作法: 透過在 powershall 執行 ```--exectime``` 以及 ```--iret``` 直接獲取執行次數分別為 0 以及為 65536 的執行時間 $t_{0}、t_{65536}$，與 retired instruction $I_{0}、I_{65536}$ ，透過以下計算獲取 host-bytes-per-guest-byte ratio $R$

$$
R = \frac{I_{65536}-I_{0}}{t_{65536} - t_{0}}
$$

| times | retired instruction | 執行時間(ms) |
| :-: | :-: | :-: |
| 0 | 12 | 0 |
| 65536 | 327692 | 22 |

$$
R = \frac{I_{65536}-I_{0}}{t_{65536} - t_{0}} = \frac{327692-12}{0.022 - 0} \approx 14.9 \times10^9 \text{  retired instruction/s}
$$
			

## 2. Shirnk the memory usage

### 2-1 Difficulties of solver.c
原本的 solver.c 採用 nonheuristic 的 BFS 算法 ( Plain BFS )，將所有狀態全部展開。由段落 "1.2 Invarient in slover.c" 中可知， `toward_solved` 將會占用約 3.504 MiB 的 private bytes 大小； `queue` 最大占用為 14.016 MiB 的 private bytes 大小；紀錄轉動後狀態的狀態轉移表 `permutation` 與 `orientation` 將會占用共 33.80 KiB 的 private bytes 大小，帶來極大記憶體開銷

我們根據在段落 "1-3 Guest performance in host" 中所測得的數據$$R_{\text{Hby/Gby, ISS}} =83 \text{  bytes/guest bytes}$$ 來估計上述三種表的 host bytes 佔用量

| Allocation | Private bytes sizes | estimated host bytes |
| :-: | :-: | :-: |
| `toward_solved` | 3.50 MiB | 290.83 MiB |
| `queue` | 14.02 MiB | 1.13 GiB |
| `permutation` + `orientation` | 33.80 KiB | 2.78 MiB |
| Computed peak | 17.55 MiB | 1.42 GiB |

根據上述表格，若強行保留完整 BFS table，host bytes 將至少占用到 1.42 GiB 以上。現在，我們採用 Heuristic Searching 的方式，來修剪原本 Plain BFS 的完整狀態圖，使其記憶體開銷降至可容許範圍，目標為: `.data`、`.bss`、`.rodata` 的 static data 合計不得超過 128 KiB。

### 2-2 What is IDA*

首先，IDA* ( Iterative Deepening A* ) 作為一種 Admissible heuristic 的算法，其核心思路如下:
1. 利用一代價函數 $f(n)=g(n)+h(n)$ 來代表搜尋的依據，詳細如下
$$
\begin{gather*}
f(n) = g(n)+h(n) \\
g(n) : \text{The actual cost of traveling from the starting point to the current node $n$} \\
h(n) : \text{Estimate the remaining cost from node $n$ to the goal.} \\
h^{ * }(n) : \text{The true shortest residual cost from node $n$ to the goal.}\\
f^{ * }(s) = f^{ * }(s) = \text{The true shortest path length from the starting point to the target state}
\end{gather*} 
$$
2. 設定一初始 threshold  $T=f(start)$
3. 執行 DFS，只要路徑上任何節點 $n$ 的 $f(n) > T$ 就立刻修剪該路徑
4. 若在當前的 $T$ 之下沒有找到解，則將 $T$ 調整為被修剪的路徑中，最小的 $f(n)$，然後再開始下一輪 DFS

對於 IDA* 為何總能尋找到最短路徑，我們使用反證法證明如下
1. 假設最短路徑長度為 $C^*$，現在 IDA* 找到了非最短路徑解的節點 $G^{'}$，其長度 $C^{'}>C^{*}$。因為 $G'$ 已經是目標節點，其 remaining cost $h(G') = 0$，所以：$$f(G') = g(G') + h(G') = C' + 0 = C' > C^*$$ 並且基於上述核心思路的第三點，IDA* 要能看見或到達節點 $G'$，該輪搜尋的 $T$ 必須至少達到：$$\text{threshold} \ge C'$$
2. 現在，選擇一條已被證明為最短的路徑，挑選一個未展開的中間節點 $n$，該節點真正的最佳總 cost 為 $C^*$，即：$$g(n) + h^*(n) = C^*$$ 而由於演算法本身必須為 Admissible，因此 $$h(n) \le h^*(n)$$
最終可得節點 $n$ 的$f(n)$ 必然小於或者等於 $C^*$
3. 當 $T$ 增加到 $T = C^*$ 的搜尋輪次時，非最短路徑解的節點 $G'$ 的代價是 $f(G') = C' > C^*$，所以在這一輪它必被修剪，不可能被造訪。反之，真正最短路徑上的所有節點，其 $f(n) \le C^*$，完全不會被修剪。
4. 而既然最佳路徑上的節點在 $T = C^*$ 這一輪都能順利通過門檻，DFS 必定會一路探索到真正的最佳目標節點 $G$。IDA* 在門檻達到 $C^{*}$ 時就會終止並回傳最短路徑，這與一開始的假設「IDA* 找到了非最短路徑解的節點 $G^{'}$」相互矛盾
5. 因此，IDA* 永遠不會找到非最短路徑解的節點 $G^{'}$

---

### 2-3 Choose IDA* and take permutation & orientation table as pattern database

我們選擇將 `p[CUBIES]` `o[CUBIES]`作為我們的 pattern database ( PDB )，分開建表以降低因 3674160 種狀態所帶來的較大記憶體開銷。兩種表皆可算出各自的 $h(s)$，分別為 $h_{p}(s)$ 與 $h_{o}(s)$，最後本倫次所選擇的 $h(s)$ 必須取嚴格上界，即$$h(s) = \max(h_o, h_p)$$

$h(s)$ 依然是 Admissible，並且相較於單純只使用 orientation table 或者 permutation table，heuristic 下界大幅收緊，IDA* 展開的節點數會大幅減少，搜尋速度大幅飆升。並且在原版 BFS中，`toward_solved` 的大小約為 3.67 MiB， `queue` 的大小約為 51.4 MiB，總和約為 55 MiB，而在 IDA* 中，`o_pdb` 約為 729 Bytes `p_pdb` 約為 5,040 B ，總和約為 5.7 KiB，記憶體開銷大幅下降。因此，接下來我們選擇 IDA* 並配合 permutation & orientation 的 PDB 以修剪原 searching tree，作為我們優化 solver.c 的方向。



本段落的最後將對效能的下限進行粗估，以作為下一段落優化 solver.c 的參考目標。根據要求:
* 搜尋每個狀態節點最多只能出現 $200$ 條 retired instructions
* 整棵 searching tree 最多只能有 $5 \times 10^7$ 條 retired instructions
* 整棵 searching tree 大概只能展出 $2.5 \times 10^5$ 個狀態節點，而原 searching tree 共有 $653,034,700$ 個節點
* 可得知新的 searching tree 必須比原本的縮小 2,612 倍

## 3. IDF_solver.c and optimization
本段落首先展示針對 solver.c 的優化版本 "IDF_solver.c"，並針對出現在 IDF_solver.c 中無法高效進行的算術 (如branch、乘法等)進行改進，以更進一步提高 IDF_solver.c 的效能。

### 3.1 Detail on IDF_solver.c
* rank : 相較於 solver.c 中使用 compisite rank，在 IDF_solver.c 中我們將 permutation 與 oritation 分開編碼，如下
```clike=
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
```
```clike=
/* 將前 6 個角塊 729 個朝向進做 rank */
static inline int rank_orientation(const state_t *state){
    int rank = 0;
    for(int i = 0; i < 6; i++){
        rank = rank * 3 + state->o[i];
    }
    return rank;
}
```
---
heuristic: 對 permutation 以及 orientation 取 heuristic 的最大值，作為當前的 $h(n)$
```clike=
/* 對 permutation 以及 orientation 取 heuristic 的最大值 */
static inline uint8_t heuristic(const state_t *s){
    uint8_t h_p = p_pdb[rank_permutation(s)];
    uint8_t h_o = o_pdb[rank_orientation(s)];
    return (h_o > h_p) ? h_o : h_p;
}

```

---
build PDB: 我們透過 ```init_p_pdb``` 與 ```init_o_pdb``` 來建立 permutation 與 orientation 的 PDB

```clike=
static void init_p_pdb(void){
    memset(p_pdb, 0xFF, sizeof(p_pdb));     // 初始化未走訪標記(0xFF)
    state_t q[P_STATES];                    //  BFS queue (只是host建表時的暫存 queue, 大小為 14*5040 = 70,560 bytes, 不會算入靜態資料大小)
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
```
```clike=
/* 對 o_pdb 透過 BFS 預先建表 */
static void init_o_pdb(void){
    memset(o_pdb, 0xFF, sizeof(o_pdb));      // 初始化未走訪標記(0xFF)
    state_t q[O_STATES];                    // BFS queue (只是host建表時的暫存 queue, 大小為 14*729 = 10,206 bytes , 不會算入靜態資料大小)
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
```

IDA DFS : 透過 ```int f = g + h``` 建立代價函數。首先在每個展開的 DFS 中先確認本輪的 ```f``` 是否超過本輪上限，若超過但是小於下一輪的代價上限 ```*next_bound```，則更新下一輪的```*next_bound```(讓代價緊緻化)，並進行下一輪的 DFS。接著測試當前狀態是否為solved state，由於門檻值是由小到大單調遞增，第一次命中此條件的路徑，在數學上保證為全域最短路徑。此時直接回傳當前的實際步數 $g$

接著對當前狀態三個面與三個轉動方式做測試，首先對轉動 face 的動作進行剪枝，我們用 ```last_f```紀錄上一步是哪一個 face 轉動，如果本次轉動的 face 與上一個轉動的 face 相同，代表本次轉動無效，因為內層迴圈已經為我們舉出單一一個 face 可轉的三種 quarter 情況，因此下一步絕不可再次旋轉同一個面（例如轉了 $R$ 之後又轉 $R2$，等同於單次轉 $R'$，會造成重覆探索）。

最後，記錄走法軌跡 ```solution_path[g]```：以深度 $g$ 為索引記錄該步動作。若底下的子遞迴回傳失敗，同一個 $g$ 之後會被其他分支自然覆寫；若回傳 ```ret >= 0 ``` 成功，該路徑即刻定稿並一路回傳，完整保留還原步驟



```clike=
// 進行 IDA* 的 DFS
static int ida_dfs(state_t s, int g, int bound, int last_f, int *next_bound) {
    uint8_t h = heuristic(&s);
    int f = g + h;

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
            /* 成功找到解答，若 ret > 0 則將實際步數沿著 Call Stack 網上傳*/
            int ret = ida_dfs(next, g + 1, bound, face, next_bound);
            if (ret > 0)
                return ret;
        }
    }
    return -1;
}
```

### 3.2 Result test
我們使用一個額外的測試用程式 "verify_ida.c"，來對 IDA_solver.c 進行 H1、H2、H3 的測試。verify_ida.c 編譯指令採用 `gcc -O3 -std=c99 -Wall -Wextra -Wpedantic .\verify_ida.c -o .\verify_ida.exe`

* H1 - Heuristic admissibility: 主要測試所有 3,674,160 個狀態是否皆合法，對每個狀態比較 `h(s) = max(h_p(s), h_o(s))` 與 BFS oracle 的精確距離 `d(s)`
![image](https://hackmd.io/_uploads/Bym9ran5Gx.png)
由上述結果可知，全部狀態皆合法

* H2 - PDB 完整性 : 主要測試兩張 PDB 的所有 entry 是否都已填入，以及 solved entry 是否為 0。詳細測試細節於 verify_ida.c 中的 `check_pdb`。以下展示測試結果![image](https://hackmd.io/_uploads/ryB68Th9fx.png)
由上述結果可知，兩種 PDB 皆已填入，並且permutation 以及 orientation 的最大深度分別為 7 與 6

* H3 - IDA* shortest solution : 證明 IDA_solver.c 與原 solver.c 的3,674,160 個狀態所需的最短路徑數全部相同，以及確認 IDA_solver.c 中的路徑是否可以真正走回 solved state。詳細測試細節於 verify_ida.c 中的 `check_h3`。以下展示測試結果
![image](https://hackmd.io/_uploads/ryTJ102czx.png)
由上述結果可知，所有 3,674,160 個狀態的最短路徑數皆與原 solver.c 相同。此外，`verify_ida.exe --h3` 的總 wall-clock time 為 21 分 26.39 秒，包含 PDB 初始化、BFS oracle 建表與精確距離計算，以及所有狀態的 IDA* 驗證

另外，我們測試了 solved state、distance - 1、distance - 11 狀態下的 `ida_dfs()` 呼叫次數，以及搜尋期間的 `quarter_turn()` 呼叫次數，如下表格。其中 state 的表示方法遵守 solver.c 中的表示方法 `PPPPPPPOOOOOOO`。本表格可做為下一段落優化 IDA_solver.c 的搜尋基準

| state | shortest path length | `ida_dfs()` calls | `quarter_turn()` calls |
|:--:|:--:|:--:|:--:|
|12345671111111|0|0|0|
|25314672313211|1|4|3|
|21345671111111|11|233966|233961|


### 3.3 IDA_solver_improv.c - A imporvement version for IDA_solver.c

在 IDA_solver.c 中，存在著一些不適合於 RV32_ISS 上運行的指令，如: mod 3 運算、recursive 運算等。在本段落中，我們嘗試針對幾種方向進行優化，以便在 Stage 4 中能夠編譯成 RISCV 的版本。我們優化的方向將包含: 引入轉移表並簡化狀態 rank 運算、改為非遞迴搜尋。簡化乘法/除法/mod運算的部分我們將留到 Stage 4 進行

---

* 引入轉移表並簡化狀態 rank 運算: 在 `ida_dfs` 中呼叫 `heuristic` 時，都會重新計算 permutation 以及 orientation 的 rank，然而類似的計算在初始化兩個 PDB 時就已經計算過了，因此這邊將會修正此問題。

首先，當前魔術方塊不再使用 `uint8_t p[C], o[C];` 表示，而是由一組新定義的 rank `uint16_t p_rank,  o_rank` 來唯一表示當魔術方塊的狀態。只需解析、計算一次 rank，之後每個搜尋節點直接用 PDB 查 heuristic，做 `quarter_turn` 時則查詢新的轉移表 `p_transition` `o_transition` 更新 rank。以下表格對比兩種方法: 使用目前的 `state_t` 搜尋、使用轉移表以及 rank 座標搜尋
|issue|use `state_t` to search|use transition table|
|:--:|:--:|:--:|
|搜尋節點表示|7個位置值與 7 個朝向值，共 14 bytes|p_rank、o_rank，共 4 bytes
|產生下一狀態|執行 quarter_turn()，改變角塊位置並更新朝向對排列 rank 和朝向| rank 各查一張轉移表|
|heuristic 查表前的工作|每個節點重新計算排列 rank 和朝向 rank，排列 rank 需要比較角塊順序|rank 已存在，直接用兩個 rank 查 PDB|
|每個 quarter-turn 的主要成本|狀態搬動、朝向運算、rank 計算|兩次表格查詢：排列一次、朝向一次

以下展示改動核心，主要引入 transition table 紀錄轉動後的狀態，以供當前的魔術方塊轉動後將會出現何種狀態進行直接查表

```clike=
/* 額外建立 transition table*/
static uint16_t p_transition[3][P_STATES];
static uint16_t o_transition[3][O_STATES];
static int transitions_ready;
static uint64_t transition_pair_calls = 0;

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

    transitions_ready = 1;  // transition table 建立完成，可供使用
}
```




對應的 h1 至 h3 測試結果如下
![image](https://hackmd.io/_uploads/SyD7X_6qzx.png)
![image](https://hackmd.io/_uploads/r1-47Opcfe.png)
![image](https://hackmd.io/_uploads/HyErQdp9fx.png)

值得關注的地方在 h3 測試中，耗時約 4 分 11.26 秒，相較於原本的 wall-clock time 提升了約 5.1 倍。接下來，測試 solved state、distance - 1、distance - 11 狀態下的 `ida_dfs()` 呼叫次數，以及搜尋期間的轉移表查詢呼叫次數
| state | shortest path length | `ida_dfs()` calls | transition pair calls |
|:--:|:--:|:--:|:--:|
|12345671111111|0|0|0|
|25314672313211|1|4|3|
|21345671111111|11|233966|233961|

可以從 transition pair calls 與原本的 ida_dfs calls 數量相同的結果，來確認搜尋樹的結構未改變，但是因為改用查詢 transition table 的方式，H3 的速度大幅上升，側面驗證目前的 IDA_solver_improv.c 在執行的時間有了顯著的加快

---
* 改為非遞迴搜尋: RV32_ISS 無法進行遞迴，因此我們將修正原本 ida_dfs 中透過呼叫遞迴來探索 searching tree 的作法，透過呼叫一個 stack 來保存每一次動作的紀錄與探索狀況

首先，我們將每個魔術方塊的執行動作都以一個 frame 為單位，結構如下
```clike=
/* 額外定義每個動作的 frame, 裡面包含的內容如下 */
typedef struct {
    coord_t state;          // 當前狀態
    coord_t turned;         // 轉動後的狀態
    uint8_t last_face;      // 上一層轉的面，用來跳過同面連轉
    uint8_t next_face;      // 下一層轉的面，返回後要繼續探索的位置
    uint8_t next_quarter;   // 該面的下一種轉法
    bool    entered;        // 該 frame 是否已經做過 heuristic / goal 檢查 
} frame_t;
```
 接著，我們在 `ida_search_iterative` 中實做了我們新版的搜尋方法，以下是幾個重點
 1. 呼叫 `frame_t stack` 來記錄每一個 frame，並用一個 `top` 來指向目前正在處理的 frame。每有一個新的 frame 被 push 進 stack 中，`top` 便會加一，因此 `top` 可視為 ida 中的 $g$
 2. 當目前的 frame 尚未被探索過時，計算 $f$ 值，如果大於 bound 則看條件更新 `next_bound`，然後 pop 回去父 frame，並標記已探索
 3. 確認以下狀況，若滿足則觸發剪枝: 當前 stack 超出最大空間、該層的所有面全部探索完、上一個 frame 與現在轉動的面相同
 4. 當前的 frame 透過 `next_face` `next_quarter`，選擇下一個動作。每開始轉動一個面 (`quarter == 1`)，就要將 `turned` 設為當前狀態，再透過轉移表套用一次轉動。
 5. 探索到新狀態時，產生子節點後，先推進父 frame 的 `next_face` 與 `next_quarter`，再把子節點 push 到 stack。父 frame 後續能在子節點返回後繼續探索下一個動作
 6. 每次產生子節點時，把該 frame 的動作編碼存入 solution_path[top]。找到目標時，top 是解答長度，路徑中的前 top 個動作就是解答路徑
 
改進後，一樣進行 h1~h3 測試
![image](https://hackmd.io/_uploads/HJ-TET0cGe.png)


接下來，測試 solved state、distance - 1、distance - 11 狀態下的 transition table 呼叫次數。這次，我們將 ida_dfs 的呼叫次數改為 node 的 search 次數，結果如下:
| state | shortest path length | Times of node search | transition pair calls |
|:--:|:--:|:--:|:--:|
|12345671111111|0|0|0|
|25314672313211|1|4|3|
|21345671111111|11|233966|233961|

與原版遞迴版本的 `ida_dfs` 結果相同，但是現在的版本成功轉變成非遞迴，具被可直接轉譯成 RISCV 的條件

---

## 4. Implementation on RV32I

本段落將實作 c code 的 RV32I 轉譯，首先會將`p_pdb`、`o_pdb`、`p_transition`、`o_transition` 作為靜態資料輸出。然後將搜尋的部分轉譯成 RV32I

### 4.1 Output of pdb & transition table
額外在 IDA_solver_improv.c 的 main 中新增了能輸出 `p_pdb`、`o_pdb`、`p_transition`、`o_transition` 的指令，使用 `--output_table` 進行，將會輸出含有 .data 部分的 .s 檔，以供接下來的 RV32I 轉譯作使用

上述的 `p_pdb`、`o_pdb`、`p_transition`、`o_transition`，所占記憶體分別為 5040 bytes、729 bytes、30240 bytes、4374 bytes，總和(加上 o_pdb 的 1 bytes padding)為 40384 bytes 

---

### 4.2 Memory of .data in minirubik_solver.s

.data 中含有以下主要的資料結構與 table，對應的占用記憶體如下表

| 項目 | Bytes |
|---|---:|
| permutation PDB | 5,040 |
| orientation PDB | 729 |
| permutation transitions | 30,240 |
| orientation transitions | 4,374 |
| search_stack：16 × 12 | 192 |
| solution_path | 11 |
| state_p 、 state_o | 14 |

---

### 4.3 Workflow in minirubik_solver.s

我們將 IDA_solver_improv.c 轉譯成 RV32I，其成果置於.\Ripes code\minirubik_solver.s 裡。整體流程可以大致分為如下
1. 由 IDA_solver_improv.c 透過 `.\IDA_solver_improv_v3.exe --minirubik_table_output` 在 .\Ripes code 資料夾中輸出含有上述四種 table 的 minirubik_table.s
2. minirubik_solver.s 紀錄 minirubik_table.s 中的 table 資訊於 .data 區域，隨後配置所需陣列:  `search_stack` `solution_path` `input_state` `state_p` `state_o`
3. 在 main 中透過呼叫 function `string_to_state_p` `string_to_state_o`  來將 `input_state` 的 state 資訊儲存在 `state_p` `state_o` 中，並呼叫  `parse_state` 檢驗合法性
4. 透過呼叫 `rank_permutation` `rank_orientation` 來將 state 編碼成 p_rank 以及 o_rank
5. 呼叫 `stack_initialization` 進行 stack 初始化
6. 呼叫 `coord_heuristic` 計算 heuristic
7. 呼叫 `ida_visit` 進行迭代搜尋，直到找到正確路徑
8. 驗證正確性並輸出路徑

下表為各 Register 主要用途，在不同 function 區塊的可能會有其他的使用目的，但是大致遵從以下表格。跨函式呼叫仍需保留的搜尋資訊放在 s 暫存器；每個節點自己的資訊放在 frame；t 和 a 暫存器只作短暫運算。 呼叫 helper 後，需要的 frame 位址就從 s3 和 s0 重新計算，不依賴可能已被改寫的 t0。

| 暫存器 | 固定用途 |
|:--:|:--:|
| `s0` | stack top，也就是目前搜尋深度 `g` |
| `s1` | 本輪 IDA* 的 `bound` |
| `s2` | 下一輪的 `next_bound` |
| `s3` | `search_stack` 起始位址 |
| `t0`–`t6` | 暫存計算；呼叫 helper 後視為可能已被改寫 |
| `a0`、`a1` | helper 輸入／輸出；heuristic 用它們傳入兩個 rank |
| `a2`、`a3` | `HTM_Move` 使用的轉移表基底位址 |
| `a4` | face：`0=R, 1=B, 2=D` |
| `a5` | quarter-turn 次數：`1`、`2`、`3` |
| `a6` | `HTM_Move` 內部剩餘轉動次數 |

---

### 4.4 Input encoding and validity checks

* 在 `input_state` ，固定前七位為角塊的 permutation，後七位為角塊的 orientation。因此首先需要檢查 state 資訊解析後，長度總和是不是 14，若不是14，將會觸發錯誤
* 確認正確後，將會進入 `parse_state_loop`。首先字元減去 ASCII '1'，轉為內部的 0..6、0..2，然後檢查幾種條件: 角塊編號不可超出 1~7、角塊朝向不可超出 1~3、角塊不可重複出現、朝向總和是否為 3 的倍數，其中一項不符合將會直接出現 error。
* 確認無誤後計算 o_sum += o，並重複 `parse_state_loop` 循環，直到所有 state 檢查完畢後進入 `parse_state_o_sum_check` 檢查朝向總和
* 值得注意的是，這邊檢查朝向總和時，我們透過以下方法改寫了 c code 中 `if (o_sum % 3 != 0)` 需要取 mod 的運算。
    * 用 `seen` 作為 bitmask 使用 shift、andi、or 檢查重複。
朝向總和透過反覆減 3 檢查，取代 mod 3 運算

---

### 4.5 Rank Calculation and Arithmetic Rewriting
* 在 `rank_permutation` 中，設定初值後會進到外層迴圈的 `rank_permutation_loop` 中，並透過呼叫 `p_state_smaller` 進到內層迴圈中計算 `smaller`，回傳後計算 p = p * (C - i) + smaller。

* 由於 RV32I 中並沒有乘法運算，因此我們實現 p = p * (C - i) + smaller 的方法大致如下: 首先設定一個乘法次數計數器 multiplier，然後進入 `rank_permutation_multiply` 進行重複加法，直到 multiplier 達到目標累加次數，加上 smaller 後離開內層迴圈。

* 在 `rank_orientation` 中，設定初值後進入迴圈計算 rank = rank * 3 + state->o[i]，與 `rank_permutation` 類似，這邊一樣使用 `rank_orientation_loop` 進行重複加法來實現乘法

---

### 4.6  ida_visit in RV32I

* 由於每個 frame 佔 12 bytes，因此首先計算 frame address = s3 + top * 12，透過對 top 左移三 bits 和 2 bits 並相加來實現 top*12。
* 檢查是否已完成首次進入的處理，若是首次進入該 frame，則需要進行以下動作
    * 計算 h(state)
    * 重新計算因呼叫 `coord_heuristic` 而導致 base address 被覆寫的t0
    * 計算 f = g + h 以用來判斷是否剪枝
    * 確認是否轉回了 solved state
    * 最後進到 `expand_todo` 進行 IDA* DFS 的例行事項
    
* `expand_todo`的動作包含選擇 HTM move、查轉移表、寫入路徑並 push 子節點等，而如果超過門檻 next_bound 時，會進行 `cutoff` 進行剪枝，並視情況更新門檻值，最後 pop 目前節點後回到父 frame。

---

### 4.7  Validation and Performance Records
接下來，我們將執行 T5、T6、T7 測試，以確認 minirubik_solver.s 中的 RV32I code 所計算出的最短路徑是否能夠返回 solved state、是否能正確輸出最短路徑長度、`--iret` 所獲得的 retired instruction 是否皆在 $5 \times 10^7$ 內。


* T5 : 在 Ripes 中，驗證每個受測輸入的回傳路徑確實還原方塊。為了同時能檢驗所有 distance-11 case 皆能在 $5 \times 10^7$ 個 retired instruction 中完成，我們透過一組額外的程式 "export_distance11.c" ，從原始 solver.c 的完整 BFS table 取得了 distance-11 的 2644 個 case，作為我們進行 T5 測試的測資
    * 關於測試腳本的使用，請參考 .\tests\README_T5 的說明
    * 測試腳本中，每組測試除了確認 guest 回報 T5 PASS、s8=1，以及輸出動作數與 s6 記錄的解長度一致外，host 測試腳本也會使用獨立的角塊位置與朝向規則，重新套用輸出的動作序列，確認最後回到已解狀態。
    * 根據測試結果，雖然所有 distance-11 case 皆通過 T5 測試，但是部分 distance-11 case 的 retired instructions 超過 $5\times10^7$，因此目前版本未完全達成作業要求的指令數上限。我們保留現有實作，並呈現完整測量結果。
    * 超過 retired instructions 上限的 case 如下表，共計十筆測資未符合，指令數限制符合率為 99.62%
    
    |input|iret|
    |:--:|:--:|
    |12745631111111|64637567|
    |14325671111111|75050065|
    |21354672313211|72442473|
    |25714632313211|63923670|
    |41625372313211|61629524|
    |41752632313211|73214958|
    |45312672313211|62763381|
    |47265312313211|61548327|
    |52341671111111|64185337|
    |54721631111111|76349062|
    
    * 超過 retired instructions 上限的 case 中，最大值為 "54721631111111" 的組合，retired instructions 為 76349062，約為上限的 1.53 倍