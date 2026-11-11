[CmdletBinding()]
param(
    [ValidateRange(0, 2147483647)]
    [int]$RootProcessId = 0,
    [switch]$Watch,
    [ValidateRange(1, 10000)]
    [int]$Samples = 1,
    [ValidateRange(1, 3600)]
    [int]$IntervalSeconds = 5,
    [ValidateRange(0, 3600)]
    [int]$WarmupSeconds = 0,
    [ValidateRange(0, 1048576)]
    [double]$WorkingSetBudgetMiB = 0,
    [ValidateRange(0, 1048576)]
    [double]$PrivateCommitBudgetMiB = 0,
    [switch]$FailOnBudgetExceeded,
    [string]$CsvPath
)

$ErrorActionPreference = 'Stop'
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'This memory measurement script requires Windows.'
}
if ($Watch -and $FailOnBudgetExceeded) {
    throw '-FailOnBudgetExceeded is intended for bounded -Samples runs, not indefinite -Watch mode.'
}

Write-Host 'WebCom memory: host process plus its WebView2 descendants.'
Write-Host 'Working-set totals can count shared pages more than once. Private commit is allocated private memory, not physical RAM in use.'
Write-Host 'No command lines, page titles, URLs, credentials, or browsing data are collected.'
if ($PrivateCommitBudgetMiB -gt 0) { Write-Host ("Private-commit budget: {0:N2} MiB" -f $PrivateCommitBudgetMiB) }
if ($WorkingSetBudgetMiB -gt 0) { Write-Host ("Working-set budget: {0:N2} MiB" -f $WorkingSetBudgetMiB) }

$warmupApplied = $false
$completedSamples = 0
$summaries = [Collections.Generic.List[object]]::new()
$budgetExceeded = $false

function Get-WebComMemorySample {
    # Deliberately omit Win32_Process.CommandLine so the QA artifact never
    # captures visited URLs or other browser-launch arguments.
    $processSnapshot = @(Get-CimInstance Win32_Process -Property @('ProcessId', 'ParentProcessId', 'Name', 'CreationDate'))
    $processIndex = @{}
    foreach ($processEntry in $processSnapshot) {
        $processIndex[[int]$processEntry.ProcessId] = $processEntry
    }

    $browserRoots = @($processSnapshot | Where-Object {
        $_.Name -ieq 'WebCom.exe' -and ($RootProcessId -eq 0 -or [int]$_.ProcessId -eq $RootProcessId)
    })
    if ($browserRoots.Count -eq 0) { return $null }

    $rootIds = [Collections.Generic.HashSet[int]]::new()
    foreach ($browserRoot in $browserRoots) { [void]$rootIds.Add([int]$browserRoot.ProcessId) }
    $selectedIds = [Collections.Generic.HashSet[int]]::new()
    foreach ($browserRoot in $browserRoots) { [void]$selectedIds.Add([int]$browserRoot.ProcessId) }

    foreach ($candidate in $processSnapshot) {
        if ($candidate.Name -ine 'msedgewebview2.exe') { continue }
        $current = $candidate
        $visited = [Collections.Generic.HashSet[int]]::new()
        while ($current -and $visited.Add([int]$current.ProcessId)) {
            if ($rootIds.Contains([int]$current.ProcessId)) {
                [void]$selectedIds.Add([int]$candidate.ProcessId)
                break
            }
            $parent = $processIndex[[int]$current.ParentProcessId]
            if (-not $parent) { break }
            # A recycled parent PID must not attach an unrelated process tree.
            if ($parent.CreationDate -and $current.CreationDate -and $parent.CreationDate -gt $current.CreationDate) { break }
            $current = $parent
        }
    }

    [long]$totalWorkingSet = 0
    [long]$totalPrivateCommit = 0
    [long]$hostWorkingSet = 0
    [long]$hostPrivateCommit = 0
    [long]$webViewWorkingSet = 0
    [long]$webViewPrivateCommit = 0
    $webViewProcessCount = 0
    $records = @(
        foreach ($processIdValue in $selectedIds) {
            $processObject = $null
            try {
                $processObject = Get-Process -Id $processIdValue -ErrorAction Stop
                if ($processObject.ProcessName -ine 'WebCom' -and $processObject.ProcessName -ine 'msedgewebview2') { continue }
                $snapshotEntry = $processIndex[$processIdValue]
                if ($snapshotEntry.CreationDate) {
                    $ageDifference = [Math]::Abs(($processObject.StartTime.ToUniversalTime() - $snapshotEntry.CreationDate.ToUniversalTime()).TotalSeconds)
                    if ($ageDifference -gt 2) { continue }
                }
                [long]$workingSet = $processObject.WorkingSet64
                [long]$privateCommit = $processObject.PrivateMemorySize64
                $totalWorkingSet += $workingSet
                $totalPrivateCommit += $privateCommit
                if ($processObject.ProcessName -ieq 'WebCom') {
                    $hostWorkingSet += $workingSet
                    $hostPrivateCommit += $privateCommit
                }
                else {
                    ++$webViewProcessCount
                    $webViewWorkingSet += $workingSet
                    $webViewPrivateCommit += $privateCommit
                }
                [pscustomobject]@{
                    PID = $processIdValue
                    ParentPID = [int]$snapshotEntry.ParentProcessId
                    Process = $processObject.ProcessName
                    WorkingSetMiB = [Math]::Round($workingSet / 1048576.0, 2)
                    PrivateCommitMiB = [Math]::Round($privateCommit / 1048576.0, 2)
                }
            }
            catch {
                # A renderer can terminate between the snapshot and measurement.
                Write-Verbose "Skipped PID ${processIdValue}: $($_.Exception.Message)"
            }
            finally {
                if ($processObject) { $processObject.Dispose() }
            }
        }
    )

    $workingSetMiB = [Math]::Round($totalWorkingSet / 1048576.0, 2)
    $privateCommitMiB = [Math]::Round($totalPrivateCommit / 1048576.0, 2)
    $workingSetOver = $WorkingSetBudgetMiB -gt 0 -and $workingSetMiB -gt $WorkingSetBudgetMiB
    $privateCommitOver = $PrivateCommitBudgetMiB -gt 0 -and $privateCommitMiB -gt $PrivateCommitBudgetMiB
    $summary = [pscustomobject]@{
        Timestamp = (Get-Date).ToString('o')
        RootProcessIds = (($browserRoots | ForEach-Object { [int]$_.ProcessId } | Sort-Object) -join ';')
        ProcessCount = $records.Count
        WebViewProcessCount = $webViewProcessCount
        WorkingSetMiB = $workingSetMiB
        PrivateCommitMiB = $privateCommitMiB
        HostWorkingSetMiB = [Math]::Round($hostWorkingSet / 1048576.0, 2)
        HostPrivateCommitMiB = [Math]::Round($hostPrivateCommit / 1048576.0, 2)
        WebViewWorkingSetMiB = [Math]::Round($webViewWorkingSet / 1048576.0, 2)
        WebViewPrivateCommitMiB = [Math]::Round($webViewPrivateCommit / 1048576.0, 2)
        WorkingSetBudgetMiB = $WorkingSetBudgetMiB
        PrivateCommitBudgetMiB = $PrivateCommitBudgetMiB
        BudgetExceeded = ($workingSetOver -or $privateCommitOver)
        Records = $records
    }
    return $summary
}

while ($Watch -or $completedSamples -lt $Samples) {
    $summary = Get-WebComMemorySample
    if (-not $summary) {
        if ($Watch) {
            Write-Host "$(Get-Date -Format T) Waiting for WebCom.exe..."
            Start-Sleep -Seconds $IntervalSeconds
            continue
        }
        throw 'No matching WebCom.exe process is running. Launch WebCom first, or check -RootProcessId.'
    }

    if (-not $warmupApplied -and $WarmupSeconds -gt 0) {
        $warmupApplied = $true
        Write-Host "Warmup: waiting $WarmupSeconds second(s) before the first recorded sample..."
        Start-Sleep -Seconds $WarmupSeconds
        continue
    }
    $warmupApplied = $true

    ++$completedSamples
    $summaries.Add($summary)
    if ($summary.BudgetExceeded) { $budgetExceeded = $true }

    Write-Host ''
    Write-Host ("Sample {0}: {1}" -f $completedSamples, $summary.Timestamp)
    $summary.Records | Sort-Object -Property @('Process', 'PID') | Format-Table -AutoSize | Out-Host
    $budgetText = if ($summary.BudgetExceeded) { ' | BUDGET EXCEEDED' } else { '' }
    Write-Host ("TOTAL: {0} processes ({1} WebView2) | {2:N2} MiB working set | {3:N2} MiB private commit{4}" -f `
        $summary.ProcessCount, $summary.WebViewProcessCount, $summary.WorkingSetMiB, $summary.PrivateCommitMiB, $budgetText)

    if ($CsvPath) {
        $absoluteCsvPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($CsvPath)
        $csvParent = Split-Path -Parent $absoluteCsvPath
        if ($csvParent -and -not (Test-Path -LiteralPath $csvParent)) {
            New-Item -ItemType Directory -Path $csvParent -Force | Out-Null
        }
        $csvRecord = $summary | Select-Object * -ExcludeProperty Records
        if (Test-Path -LiteralPath $absoluteCsvPath) {
            $csvRecord | Export-Csv -LiteralPath $absoluteCsvPath -NoTypeInformation -Append -Encoding UTF8
        }
        else {
            $csvRecord | Export-Csv -LiteralPath $absoluteCsvPath -NoTypeInformation -Encoding UTF8
        }
    }

    if ($Watch -or $completedSamples -lt $Samples) { Start-Sleep -Seconds $IntervalSeconds }
}

if (-not $Watch -and $summaries.Count -gt 0) {
    $working = @($summaries | ForEach-Object { [double]$_.WorkingSetMiB })
    $private = @($summaries | ForEach-Object { [double]$_.PrivateCommitMiB })
    $workingAverage = ($working | Measure-Object -Average).Average
    $privateAverage = ($private | Measure-Object -Average).Average
    $workingPeak = ($working | Measure-Object -Maximum).Maximum
    $privatePeak = ($private | Measure-Object -Maximum).Maximum
    Write-Host ''
    Write-Host ("SUMMARY: {0} samples | avg working set {1:N2} MiB | peak {2:N2} MiB | avg private commit {3:N2} MiB | peak {4:N2} MiB" -f `
        $summaries.Count, $workingAverage, $workingPeak, $privateAverage, $privatePeak)
}

if ($FailOnBudgetExceeded -and $budgetExceeded) {
    Write-Error 'WebCom memory regression gate failed because at least one recorded sample exceeded the configured budget.'
    exit 2
}
