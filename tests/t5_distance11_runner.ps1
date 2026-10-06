param(
    [string]$StatesPath = 'C:\Learning\programing_project\C\computer_architecture\HW1\minirubik\distance11_states.txt',
    [string]$SourcePath = 'C:\Learning\programing_project\C\computer_architecture\HW1\minirubik\Ripes code\minirubik_solver.s',
    [string]$RipesPath = 'C:\Learning\programing_project\Ripes RISC-V Simulator\Ripes-continuous\Ripes.exe',
    [string]$OutputDirectory = (Join-Path $PSScriptRoot 't5_distance11_results'),
    [ValidateRange(0,2644)][int]$MaxCases = 0,
    [ValidateRange(1000,3600000)][int]$TimeoutMs = 120000,
    [switch]$Resume
)

$ErrorActionPreference = 'Stop'
$taskUtf8 = [System.Text.UTF8Encoding]::new($false)
$taskInputs = @(Get-Content -LiteralPath $StatesPath -Encoding UTF8)
if ($taskInputs.Count -ne 2644 -or @($taskInputs | Sort-Object -Unique).Count -ne 2644) {
    throw 'The state list must contain exactly 2644 unique lines.'
}
foreach ($taskInput in $taskInputs) {
    if ($taskInput -notmatch '^[1-7]{7}[1-3]{7}$') { throw ('Invalid input line: ' + $taskInput) }
    if ((-join ($taskInput.Substring(0,7).ToCharArray() | Sort-Object)) -ne '1234567') {
        throw ('Invalid cubie permutation: ' + $taskInput)
    }
    $taskOrientationSum = 0
    for ($taskI = 7; $taskI -lt 14; ++$taskI) { $taskOrientationSum += [int][char]$taskInput[$taskI] - 49 }
    if ($taskOrientationSum % 3 -ne 0) { throw ('Invalid orientation sum: ' + $taskInput) }
}
$taskSource = Get-Content -Raw -LiteralPath $SourcePath -Encoding UTF8
$taskInputRegex = [regex]::new('(?m)(^input_state:\s*\r?\n\s*\.string\s+")[^"]*(")')
if ($taskInputRegex.Matches($taskSource).Count -ne 1) { throw 'Expected one input_state string declaration.' }
if ($taskSource -notmatch '(?m)^\.data\s*$') { throw 'Missing .data declaration.' }

function Write-T5Text {
    param([string]$Path, [string]$Contents)
    [System.IO.File]::WriteAllText($Path, $Contents, $taskUtf8)
}

function Test-T5Replay {
    param([string]$CubeInput, [string]$Moves)
    $taskP = [int[]]::new(7)
    $taskO = [int[]]::new(7)
    for ($taskJ = 0; $taskJ -lt 7; ++$taskJ) {
        $taskP[$taskJ] = [int][char]$CubeInput[$taskJ] - 49
        $taskO[$taskJ] = [int][char]$CubeInput[$taskJ + 7] - 49
    }
    # Cubie replay does not use the guest's exported PDBs or transition tables.
    $taskSources = @(@(1,4,2,0,3,5,6), @(0,1,2,4,5,6,3), @(0,2,5,3,1,4,6))
    $taskTwists = @(@(1,2,0,2,1,0,0), @(0,0,0,1,2,1,2), @(0,0,0,0,0,0,0))
    $taskNames = @('R','R2',"R'",'B','B2',"B'",'D','D2',"D'")
    foreach ($taskToken in @($Moves -split '\s+' | Where-Object { $_.Length -gt 0 })) {
        $taskMoveId = [Array]::IndexOf($taskNames, $taskToken)
        if ($taskMoveId -lt 0) { return $false }
        $taskFace = [int][Math]::Floor($taskMoveId / 3)
        for ($taskQuarter = 0; $taskQuarter -lt ($taskMoveId % 3 + 1); ++$taskQuarter) {
            $taskNewP = [int[]]::new(7)
            $taskNewO = [int[]]::new(7)
            for ($taskJ = 0; $taskJ -lt 7; ++$taskJ) {
                $taskFrom = $taskSources[$taskFace][$taskJ]
                $taskNewP[$taskJ] = $taskP[$taskFrom]
                $taskNewO[$taskJ] = ($taskO[$taskFrom] + $taskTwists[$taskFace][$taskJ]) % 3
            }
            $taskP = $taskNewP
            $taskO = $taskNewO
        }
    }
    for ($taskJ = 0; $taskJ -lt 7; ++$taskJ) {
        if ($taskP[$taskJ] -ne $taskJ -or $taskO[$taskJ] -ne 0) { return $false }
    }
    return $true
}

function Read-T5Number {
    param([string]$Text, [string]$Pattern)
    $taskMatch = [regex]::Match($Text, $Pattern)
    if (-not $taskMatch.Success) { throw ('Missing report field: ' + $Pattern) }
    return [long]$taskMatch.Groups[1].Value
}

function Save-T5Summary {
    $taskRows = @($taskResults.Values | Sort-Object case)
    if ($taskRows.Count -gt 0) {
        $taskRows | Export-Csv -LiteralPath (Join-Path $OutputDirectory 'results.csv') -NoTypeInformation -Encoding UTF8
    }
    $taskGood = @($taskRows | Where-Object t5Passed).Count
    $taskOptimal = @($taskRows | Where-Object shortestLengthMatches).Count
    $taskOverBudget = @($taskRows | Where-Object { $null -ne $_.instructions -and $_.instructions -gt 50000000 }).Count
    $taskMeasured = @($taskRows | Where-Object { $null -ne $_.instructions } | Sort-Object instructions -Descending)
    $taskSummaryLines = @(
        '# Distance-11：T5 與指令數紀錄', '',
        ('處理器：`RV32_ISS`，RV32I，ISA extensions 空白。更新時間：' + [DateTimeOffset]::Now.ToString('o') + '。'), '',
        ('- 已記錄：**' + $taskRows.Count + '／2644**。'),
        ('- T5 路徑重播通過：**' + $taskGood + '／' + $taskRows.Count + '**。'),
        ('- 解長度為 11：**' + $taskOptimal + '／' + $taskRows.Count + '**。'),
        ('- 超過 50,000,000 retired instructions：**' + $taskOverBudget + '** 組。'), '',
        'T5 必須同時滿足：Ripes 正常結束、回報 T5 PASS、s8=1、印出的步數與 s6 相同，以及輸出路徑經獨立角塊規則重播後回到已解狀態。11 步與指令預算另行檢查。', '',
        '## 指令數最大的已測案例', '',
        '| Case（從 0 起） | Input | Instructions | T5 | Length=11 |',
        '|---:|---|---:|---|---|'
    )
    foreach ($taskRow in @($taskMeasured | Select-Object -First 10)) {
        $taskSummaryLines += '| ' + $taskRow.case + ' | `' + $taskRow.input + '` | ' + $taskRow.instructions + ' | ' + $taskRow.t5Passed + ' | ' + $taskRow.shortestLengthMatches + ' |'
    }
    if ($taskRows.Count -lt 2644) {
        $taskSummaryLines += @('', '**尚未跑完全部測資；目前最大值不能當作全部 2644 組的 worst case。**')
    }
    $taskSummaryLines += @('', '## 重現資訊', '',
        ('- 組語 SHA-256：`' + $taskManifest.sourceSha256 + '`。'),
        ('- Ripes SHA-256：`' + $taskManifest.ripesSha256 + '`。'),
        ('- 測資 SHA-256：`' + $taskManifest.statesSha256 + '`。'),
        '- source_snapshot.s、states_snapshot.txt 保存本次輸入來源；manifest.json 保存執行條件。',
        '- 每組 case_NNNN.json 與 case_NNNN_ripes.txt 保存完整結果與原始輸出。輸入狀態可由 case 編號對應至 states_snapshot.txt 第 case+1 行。',
        '- 測試副本只替換 input_state，並在 .data 開頭加入 4096-byte padding，避開本 CLI build 的 text/data 位址重疊；組語指令未修改。',
        '- 若本 build 的 T5 PASS 字串後出現一個 NUL，只去除路徑開頭的這個字元，原始輸出仍完整保存。',
        '- T5 使用獨立的角塊規則重播；距離 11 來自先前以 host BFS oracle 產生的測資清單。',
        '- 分批執行必須使用相同組語、Ripes 與測資；腳本以 SHA-256 阻止不同來源的結果混合。',
        '- T7 的 pipeline model 與已解／短打亂案例另行執行。')
    Write-T5Text (Join-Path $OutputDirectory 'summary.md') ($taskSummaryLines -join "`n")
}

$taskReplayControls = @(
    @{ input='12345671111111'; moves=''; expected=$true },
    @{ input='25314672313211'; moves="R'"; expected=$true },
    @{ input='25314672313211'; moves=''; expected=$false },
    @{ input='12345671111111'; moves='X'; expected=$false }
)
foreach ($taskControl in $taskReplayControls) {
    if ((Test-T5Replay $taskControl.input $taskControl.moves) -ne $taskControl.expected) {
        throw 'External replay checker control failed.'
    }
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path
$taskLock = [System.IO.File]::Open((Join-Path $OutputDirectory 'run.lock'), [System.IO.FileMode]::OpenOrCreate,
    [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
$taskResults = @{}
$taskFailedThisRun = $false
try {
    $taskManifestPath = Join-Path $OutputDirectory 'manifest.json'
    $taskManifest = [ordered]@{
        createdAt = [DateTimeOffset]::Now.ToString('o')
        processor = 'RV32_ISS'
        isaExtensions = @()
        sourcePath = $SourcePath
        sourceSha256 = (Get-FileHash -LiteralPath $SourcePath -Algorithm SHA256).Hash
        statesPath = $StatesPath
        statesSha256 = (Get-FileHash -LiteralPath $StatesPath -Algorithm SHA256).Hash
        ripesPath = $RipesPath
        ripesSha256 = (Get-FileHash -LiteralPath $RipesPath -Algorithm SHA256).Hash
        totalCases = 2644
        cliPaddingBytes = 4096
        scriptSha256 = (Get-FileHash -LiteralPath $PSCommandPath -Algorithm SHA256).Hash
    }
    if (Test-Path -LiteralPath $taskManifestPath) {
        if (-not $Resume) { throw 'Results already exist. Use -Resume, or specify a new -OutputDirectory.' }
        $taskPrevious = Get-Content -Raw -LiteralPath $taskManifestPath -Encoding UTF8 | ConvertFrom-Json
        foreach ($taskField in @('sourceSha256','statesSha256','ripesSha256','processor','scriptSha256')) {
            if ($taskPrevious.$taskField -ne $taskManifest[$taskField]) { throw ('Resume source mismatch: ' + $taskField) }
        }
        $taskManifest = $taskPrevious
        foreach ($taskResultFile in @(Get-ChildItem -LiteralPath $OutputDirectory -Filter 'case_????.json')) {
            $taskSaved = Get-Content -Raw -LiteralPath $taskResultFile.FullName -Encoding UTF8 | ConvertFrom-Json
            if ($taskSaved.case -lt 0 -or $taskSaved.case -ge 2644 -or $taskSaved.input -ne $taskInputs[$taskSaved.case]) {
                throw ('Invalid saved case: ' + $taskResultFile.Name)
            }
            $taskResults[[int]$taskSaved.case] = $taskSaved
        }
    } else {
        if (@(Get-ChildItem -LiteralPath $OutputDirectory -Filter 'case_????.json').Count -gt 0) { throw 'Case records exist without a manifest.' }
        Write-T5Text $taskManifestPath ($taskManifest | ConvertTo-Json -Depth 5)
        Write-T5Text (Join-Path $OutputDirectory 'source_snapshot.s') $taskSource
        Copy-Item -LiteralPath $StatesPath -Destination (Join-Path $OutputDirectory 'states_snapshot.txt')
    }

    # The existing CLI build starts both sections at zero. Keep data above text.
    $taskPadding = '# CLI layout padding: 4096 bytes.' + "`n" + (('    .word 0, 0, 0, 0' + "`n") * 256)
    $taskTemplate = [regex]::new('(?m)^\.data\s*\r?\n').Replace($taskSource, ('.data' + "`n" + $taskPadding), 1)
    $taskCaseSourcePath = Join-Path $OutputDirectory 'current_case.s'
    $taskRunCount = 0
    Save-T5Summary

    for ($taskIndex = 0; $taskIndex -lt 2644; ++$taskIndex) {
        if ($taskResults.ContainsKey($taskIndex)) { continue }
        if ($MaxCases -gt 0 -and $taskRunCount -ge $MaxCases) { break }
        $taskInput = $taskInputs[$taskIndex]
        $taskCaseSource = $taskInputRegex.Replace($taskTemplate, [System.Text.RegularExpressions.MatchEvaluator]{
            param($taskMatch)
            return $taskMatch.Groups[1].Value + $taskInput + $taskMatch.Groups[2].Value
        }, 1)
        Write-T5Text $taskCaseSourcePath $taskCaseSource
        $taskRawPath = Join-Path $OutputDirectory ('case_{0:D4}_ripes.txt' -f $taskIndex)
        $taskResult = [ordered]@{
            case=$taskIndex; input=$taskInput; timestamp=[DateTimeOffset]::Now.ToString('o')
            solution=''; solutionLength=$null; printedMoveCount=$null
            guestS8=$null; guestReportedPass=$false; replayPassed=$false
            t5Passed=$false; shortestLengthMatches=$false; withinInstructionBudget=$false
            instructions=$null; modelMs=$null; processWallSeconds=$null
            timeoutMs=$TimeoutMs; nativeExitCode=$null; consoleStringNulPresent=$false; error=''
        }
        $taskClock = [System.Diagnostics.Stopwatch]::StartNew()
        try {
            $taskRaw = & $RipesPath --mode cli --src $taskCaseSourcePath -t asm --proc RV32_ISS --timeout $TimeoutMs --iret --regs --exectime --runinfo 2>&1 | Out-String
            $taskResult.nativeExitCode = $LASTEXITCODE
            $taskClock.Stop()
            $taskResult.processWallSeconds = $taskClock.Elapsed.TotalSeconds
            Write-T5Text $taskRawPath $taskRaw
            if ($taskResult.nativeExitCode -ne 0) { throw ('Ripes exit code: ' + $taskResult.nativeExitCode) }
            if ($taskRaw -match 'Unknown system call|ERROR:|aborted|timed out') { throw 'Ripes reported an error or timeout.' }
            if ($taskRaw -notmatch 'processor:\s*RV32_ISS' -or $taskRaw -notmatch '(?m)^ISA extensions:[ \t]*\r?$') {
                throw 'The report did not confirm RV32_ISS with empty ISA extensions.'
            }
            if ($taskRaw -notmatch 'Program exited with code:\s*0') { throw 'Missing normal program exit report.' }
            $taskProgramLines = @(($taskRaw -split 'Program exited with code:')[0].Trim() -split '\r?\n')
            $taskResult.guestReportedPass = $taskProgramLines[0].Trim() -eq 'T5 PASS'
            $taskMoves = (($taskProgramLines | Select-Object -Skip 1) -join ' ').Trim()
            $taskResult.consoleStringNulPresent = $taskMoves.Length -gt 0 -and $taskMoves[0] -eq [char]0
            if ($taskResult.consoleStringNulPresent) { $taskMoves = $taskMoves.Substring(1).Trim() }
            $taskResult.solution = $taskMoves
            $taskResult.printedMoveCount = @($taskMoves -split '\s+' | Where-Object { $_.Length -gt 0 }).Count
            $taskResult.solutionLength = Read-T5Number $taskRaw '(?m)^x22:\s*(-?\d+)'
            $taskResult.guestS8 = Read-T5Number $taskRaw '(?m)^x24:\s*(-?\d+)'
            $taskResult.instructions = Read-T5Number $taskRaw 'instructions retired\s+(\d+)'
            $taskResult.modelMs = Read-T5Number $taskRaw 'execution time \(ms\)\s+(\d+)'
            $taskResult.replayPassed = Test-T5Replay $taskInput $taskMoves
            $taskResult.t5Passed = $taskResult.guestReportedPass -and $taskResult.guestS8 -eq 1 -and $taskResult.replayPassed -and $taskResult.printedMoveCount -eq $taskResult.solutionLength
            $taskResult.shortestLengthMatches = $taskResult.solutionLength -eq 11
            $taskResult.withinInstructionBudget = $taskResult.instructions -le 50000000
        } catch {
            $taskClock.Stop()
            $taskResult.processWallSeconds = $taskClock.Elapsed.TotalSeconds
            $taskResult.error = $_.Exception.Message
        }
        if (-not $taskResult.t5Passed -or -not $taskResult.shortestLengthMatches) {
            Write-T5Text (Join-Path $OutputDirectory ('case_{0:D4}_failed.s' -f $taskIndex)) $taskCaseSource
            $taskFailedThisRun = $true
        }
        # Publish each complete record atomically; an interrupted case is rerun.
        $taskJsonPath = Join-Path $OutputDirectory ('case_{0:D4}.json' -f $taskIndex)
        Write-T5Text ($taskJsonPath + '.tmp') ($taskResult | ConvertTo-Json -Depth 5)
        Move-Item -LiteralPath ($taskJsonPath + '.tmp') -Destination $taskJsonPath -Force
        $taskResults[$taskIndex] = [pscustomobject]$taskResult
        ++$taskRunCount
        Write-Output ('[{0}/2644] case={1}, input={2}, T5={3}, length={4}, iret={5}, budget={6}' -f $taskResults.Count,$taskIndex,$taskInput,$taskResult.t5Passed,$taskResult.solutionLength,$taskResult.instructions,$taskResult.withinInstructionBudget)
        if ($taskRunCount % 10 -eq 0 -or $taskFailedThisRun) { Save-T5Summary }
        if ($taskFailedThisRun) { break }
    }
    Save-T5Summary
    Write-Output ('Recorded {0}/2644 cases. Summary: {1}' -f $taskResults.Count,(Join-Path $OutputDirectory 'summary.md'))
    if ($taskFailedThisRun) { throw 'A correctness case failed; inspect its JSON/raw output before continuing.' }
} finally {
    $taskLock.Dispose()
}
