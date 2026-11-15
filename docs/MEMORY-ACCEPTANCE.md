# WebCom v3 Windows memory acceptance

WebCom's memory target must be measured on Windows with the same pages, runtime version, profile state and tab state. Do not infer browser-wide RAM savings from Task Manager screenshots or a single process.

The repository includes `scripts/Measure-WebComMemory.ps1`. It measures `WebCom.exe` plus the descendant `msedgewebview2.exe` process tree without reading process command lines, page titles, URLs, credentials or browsing data.

## Required scenarios

Record at least these five scenarios for a public-beta candidate:

1. **one-tab** — one normal profile, one settled web tab, no DevTools.
2. **multi-tab** — one profile with the agreed repeated test set of active/background tabs.
3. **sleeping-tabs** — the multi-tab set after eligible background tabs visibly enter WebCom's sleeping state.
4. **split-view** — two visible web pages in WebCom split view plus the same background baseline.
5. **multi-profile** — two simultaneous WebCom profile windows using the agreed test pages.

Use the same Windows build, WebView2 Runtime, sites, zoom, extensions, blocker state and warm-up interval when comparing builds.

## Example one-tab run

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\Measure-WebComMemory.ps1 `
  -Scenario one-tab `
  -WarmupSeconds 30 `
  -Samples 12 `
  -IntervalSeconds 5 `
  -CsvPath .\artifacts\memory-one-tab.csv `
  -JsonPath .\artifacts\memory-one-tab.json
```

For a temporary regression gate, add explicit budgets only after a baseline has been approved:

```powershell
  -WorkingSetBudgetMiB 300 `
  -PrivateCommitBudgetMiB 450 `
  -FailOnBudgetExceeded
```

A budget is a project acceptance threshold, not a claim that every website will remain below it. Heavy websites and WebView2 Runtime changes can materially alter the process-group total.

## What the report contains

Each recorded sample includes:

- total WebCom + descendant WebView2 working set;
- total private commit;
- WebCom host working/private memory;
- WebView2 working/private memory;
- measured process count and WebView2 process count;
- scenario label and timestamp;
- configured budget and whether it was exceeded.

The bounded JSON report also summarizes average, median, p95 and peak values. CSV contains one row per sample.

## Interpreting the numbers

- **Working set** is currently resident memory. Shared pages can be counted in more than one process.
- **Private commit** is private allocated memory and can include pages that are not resident in physical RAM.
- A renderer can exit between process discovery and sampling. Repeat the run if the process set changes unexpectedly.
- Compare matched scenarios. Do not compare WebCom one-tab against another browser with different pages, extensions, startup state or runtime warm-up.

The in-browser **About WebCom → Performance snapshot** is useful for a quick live check. Release acceptance should use the PowerShell script because it records repeated samples and produces reviewable artifacts.
