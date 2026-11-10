[CmdletBinding()]
param(
    [ValidateRange(0, 2147483647)]
    [int]$RootProcessId = 0,
    [switch]$Watch,
    [ValidateRange(1, 3600)]
    [int]$IntervalSeconds = 5,
    [string]$CsvPath
)

$ErrorActionPreference = 'Stop'
if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
    throw 'This memory measurement script requires Windows.'
}

Write-Host 'WebCom memory: host process plus its WebView2 descendants.'
Write-Host 'Working-set totals can count shared pages more than once. Private commit is allocated private memory, not physical RAM in use.'
Write-Host 'These are measurement metrics, not a claim of savings against Chrome.'

do {
    # No command lines, page titles, credentials, or browsing data are collected.
    $processSnapshot = @(Get-CimInstance Win32_Process -Property @('ProcessId', 'ParentProcessId', 'Name', 'CreationDate'))
    $processIndex = @{}
    foreach ($processEntry in $processSnapshot) {
        $processIndex[[int]$processEntry.ProcessId] = $processEntry
    }

    $browserRoots = @($processSnapshot | Where-Object {
        $_.Name -ieq 'WebCom.exe' -and ($RootProcessId -eq 0 -or [int]$_.ProcessId -eq $RootProcessId)
    })
    if ($browserRoots.Count -eq 0) {
        if ($Watch) {
            Write-Host "$(Get-Date -Format T) Waiting for WebCom.exe..."
            Start-Sleep -Seconds $IntervalSeconds
            continue
        }
        throw 'No matching WebCom.exe process is running. Launch WebCom first, or check -RootProcessId.'
    }

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

    $sampleTimestamp = (Get-Date).ToString('o')
    [long]$totalWorkingSet = 0
    [long]$totalPrivateCommit = 0
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

    $summary = [pscustomobject]@{
        Timestamp = $sampleTimestamp
        RootProcessIds = (($browserRoots | ForEach-Object { [int]$_.ProcessId } | Sort-Object) -join ';')
        ProcessCount = $records.Count
        WorkingSetMiB = [Math]::Round($totalWorkingSet / 1048576.0, 2)
        PrivateCommitMiB = [Math]::Round($totalPrivateCommit / 1048576.0, 2)
    }
    Write-Host ''
    Write-Host $sampleTimestamp
    $records | Sort-Object -Property @('Process', 'PID') | Format-Table -AutoSize | Out-Host
    Write-Host ("TOTAL: {0} processes | {1:N2} MiB working set | {2:N2} MiB private commit" -f $summary.ProcessCount, $summary.WorkingSetMiB, $summary.PrivateCommitMiB)

    if ($CsvPath) {
        $absoluteCsvPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($CsvPath)
        $csvParent = Split-Path -Parent $absoluteCsvPath
        if (-not (Test-Path -LiteralPath $csvParent)) {
            New-Item -ItemType Directory -Path $csvParent -Force | Out-Null
        }
        if (Test-Path -LiteralPath $absoluteCsvPath) {
            $summary | Export-Csv -LiteralPath $absoluteCsvPath -NoTypeInformation -Append -Encoding UTF8
        }
        else {
            $summary | Export-Csv -LiteralPath $absoluteCsvPath -NoTypeInformation -Encoding UTF8
        }
    }
    if ($Watch) { Start-Sleep -Seconds $IntervalSeconds }
} while ($Watch)
