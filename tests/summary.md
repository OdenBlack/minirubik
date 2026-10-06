# Distance-11：T5 與指令數紀錄

處理器：`RV32_ISS`，RV32I，ISA extensions 空白。更新時間：2026-10-07T02:12:55.9972333+08:00。

- 已記錄：**2644／2644**。
- T5 路徑重播通過：**2644／2644**。
- 解長度為 11：**2644／2644**。
- 超過 50,000,000 retired instructions：**10** 組。

T5 必須同時滿足：Ripes 正常結束、回報 T5 PASS、s8=1、印出的步數與 s6 相同，以及輸出路徑經獨立角塊規則重播後回到已解狀態。11 步與指令預算另行檢查。

## 指令數最大的已測案例

| Case（從 0 起） | Input | Instructions | T5 | Length=11 |
|---:|---|---:|---|---|
| 1790 | `54721631111111` | 76349062 | True | True |
| 188 | `14325671111111` | 75050065 | True | True |
| 1336 | `41752632313211` | 73214958 | True | True |
| 540 | `21354672313211` | 72442473 | True | True |
| 108 | `12745631111111` | 64637567 | True | True |
| 1613 | `52341671111111` | 64185337 | True | True |
| 711 | `25714632313211` | 63923670 | True | True |
| 1410 | `45312672313211` | 62763381 | True | True |
| 1303 | `41625372313211` | 61629524 | True | True |
| 1541 | `47265312313211` | 61548327 | True | True |

## 重現資訊

- 組語 SHA-256：`ACA059F85AE84D1DA6FDD9BF1E803AA960DB8396B626DCDC11CA7265A8BDD253`。
- Ripes SHA-256：`BD2DDEA8CD6FCF6902CDA7366FE99AB6DD0C7FDBBC7EFCFDB89EDE20ACC67F0F`。
- 測資 SHA-256：`35FBC8415CF0A5084DDEEE6DAD7F7CBA6028593DAA2B12E18C98A4089B67F065`。
- source_snapshot.s、states_snapshot.txt 保存本次輸入來源；manifest.json 保存執行條件。
- 每組 case_NNNN.json 與 case_NNNN_ripes.txt 保存完整結果與原始輸出。輸入狀態可由 case 編號對應至 states_snapshot.txt 第 case+1 行。
- 測試副本只替換 input_state，並在 .data 開頭加入 4096-byte padding，避開本 CLI build 的 text/data 位址重疊；組語指令未修改。
- 若本 build 的 T5 PASS 字串後出現一個 NUL，只去除路徑開頭的這個字元，原始輸出仍完整保存。
- T5 使用獨立的角塊規則重播；距離 11 來自先前以 host BFS oracle 產生的測資清單。
- 分批執行必須使用相同組語、Ripes 與測資；腳本以 SHA-256 阻止不同來源的結果混合。
- T7 的 pipeline model 與已解／短打亂案例另行執行。