# T5：distance-11 批次驗證

以 host BFS oracle 產生的全部 2,644 個 distance-11 狀態，逐組執行 RV32I solver，獨立重播輸出路徑並記錄指令數。

## 檔案配置

```text
minirubik/
├── distance11_states.txt             # 2,644 行測資，已在 repo 根目錄
├── export_distance11.c               # 使用原始 solver.c 的 BFS oracle 產生測資
├── Ripes code/
│   └── minirubik_solver.s            # 受測 RV32I 程式
├── tests/
│   ├── run_t5_distance11.ps1         # 執行入口；預設輸入路徑相對於 repo
│   ├── t5_distance11_runner.ps1      # 原批次腳本；保留內容以相容既有續跑紀錄
│   ├── README_T5.md
│   └── summary.md                   # 每次批次正常結束後更新的摘要副本
└── test_results/
    └── t5_distance11/               # 預設結果資料夾，執行時建立
```

請使用 `run_t5_distance11.ps1` 作為入口。它會明確傳入所有路徑，因此執行核心內保留的舊預設路徑不會被使用。Ripes 的安裝路徑由 `-RipesPath` 指定。

## 執行方式

需要支援 `RV32_ISS`、`--iret`、`--regs`、`--exectime` 與 `--runinfo` 的 Ripes build。本機先前的批次測試使用 PowerShell 7；以下指令請在 PowerShell 7 中、repo 根目錄執行。

先設定自己的 Ripes 路徑，再執行前 3 組：

```powershell
$Ripes = 'C:\Learning\programing_project\Ripes RISC-V Simulator\Ripes-continuous\Ripes.exe'
& .\tests\run_t5_distance11.ps1 -RipesPath $Ripes -MaxCases 3
```

接著續跑剩餘案例：

```powershell
& .\tests\run_t5_distance11.ps1 -RipesPath $Ripes -Resume
```

第一次就跑全部案例，可省略 `-MaxCases`。`-MaxCases` 是本次新增案例的上限，0 表示不限；每組的 timeout 預設 120,000 ms，可用 `-TimeoutMs` 修改。

入口腳本在每次批次正常結束後，會將結果資料夾的 `summary.md` 複製到 `tests/summary.md`，並保留原始摘要。使用自訂 `-OutputDirectory` 或 `-Resume` 時也會更新這份副本；既有副本會被覆寫為最新摘要。使用 `-MaxCases` 分批測試時，副本會呈現截至本次已完成的案例數，未測完 2,644 組時也會標明尚未完成。

若目前是在 Windows PowerShell，可使用已安裝的 PowerShell 7 執行檔：

```powershell
$Pwsh = 'C:\Users\OdenBlack\.cache\codex-runtimes\codex-primary-runtime\dependencies\native\powershell\pwsh.exe'
& $Pwsh -NoProfile -File .\tests\run_t5_distance11.ps1 -RipesPath $Ripes -Resume
```

以上兩個安裝路徑是本機範例，換電腦時請替換。

## 接續原本資料夾中的進度

現有批次正在執行時，先讓它完成或停止，再用下列入口接續。指定原本結果資料夾即可讀取已完成的 JSON，跳過這些案例；無須搬移或重新執行已完成的案例。

```powershell
$ExistingResults = 'C:\Users\OdenBlack\Documents\ChatGPT\Computer Architecture\ripes_validation\t5_distance11_results'
& .\tests\run_t5_distance11.ps1 -RipesPath $Ripes -OutputDirectory $ExistingResults -Resume
```

結果資料夾有檔案鎖，同一份結果只能由一個批次程序寫入。執行核心保留與原腳本完全相同的位元組，`.gitattributes` 也禁止 Git 轉換它的換行，因此搬到 repo 後仍可通過既有 `scriptSha256` 檢查。

`-Resume` 會核對組語、Ripes 執行檔、測資及執行核心的 SHA-256。若修改這些來源，請指定新的結果資料夾，例如：

```powershell
& .\tests\run_t5_distance11.ps1 -RipesPath $Ripes -OutputDirectory .\test_results\t5_v2
```

## 每組檢查與限制

1. Ripes 正常結束，回報 `RV32_ISS`，且 ISA extensions 空白。
2. Guest 輸出 `T5 PASS`，且 `s8`（x24）為 1。
3. Host 用角塊位置與朝向規則重播印出的動作，確認回到已解狀態。此檢查不查詢 guest 的 PDB 或 transition tables。
4. 印出的動作數與 `s6`（x22）記錄的解長度相同。
5. 另外確認解長度為 11，並記錄 retired instructions 是否不超過 50,000,000。

路徑或解長度出錯時，保存失敗組語副本並停止。超過指令預算時，記錄 `withinInstructionBudget=False` 並繼續，讓整批測資的最大指令數仍能被找出。T5 通過不等於指令預算通過。

測試副本僅替換 `input_state`，並在 `.data` 開頭加入 4 KiB padding，避開目前 CLI build 的 text/data 位址重疊；原始組語檔不會被改寫。若該 build 的 `T5 PASS` 字串後印出一個 NUL，只移除路徑開頭的這個字元，原始 log 仍保留。

這批案例涵蓋全部 distance-11 狀態。已解、短打亂案例及 T7 的 pipeline model 驗證需另外記錄。

## 結果檔案

| 檔案 | 內容 |
|---|---|
| `tests/summary.md` | 入口腳本在批次正常結束後更新的摘要副本，可直接提交至 repo |
| `summary.md` | 已測數量、T5 通過數、11 步解數、超預算數及目前最大指令數案例 |
| `results.csv` | 所有已完成案例的結果 |
| `case_NNNN.json` | 單一案例的檢查結果 |
| `case_NNNN_ripes.txt` | 原始 Ripes 輸出 |
| `case_NNNN_failed.s` | 正確性失敗時保留的測試副本 |
| `source_snapshot.s`、`states_snapshot.txt` | 本次使用的組語與測資 |
| `manifest.json` | 執行條件與來源 SHA-256 |
| `current_case.s`、`run.lock` | 執行中的暫存檔與檔案鎖 |

只有已記錄數量為 2644/2644，且所有案例成功完成時，才能把最大指令數當作這批完整 domain 的實測 worst case。

`test_results/` 已加入 `.gitignore`，避免每組暫存檔與大量原始 log 被直接加入提交。`tests/summary.md` 可直接加入提交；`results.csv` 與 `manifest.json` 可複製到 `measurements/t5_distance11/` 再提交，完整原始紀錄另行保存。

## 重新產生測資

目前 repo 根目錄已有清單。需要重新產生時，在 repo 根目錄編譯 host exporter：

```powershell
gcc -O3 -std=c99 -Wall -Wextra -Wpedantic .\export_distance11.c -o .\export_distance11.exe
.\export_distance11.exe .\distance11_states.txt
```

Exporter 會包含原始 `solver.c` 並建立完整 BFS oracle，逐一挑出精確距離 11 的狀態，要求輸出恰為 2,644 組。完成後清單順序或換行格式若有變動，SHA-256 也可能改變，請以新的結果資料夾執行。
