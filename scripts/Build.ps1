[CmdletBinding()]
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release',
    [ValidateSet('x64', 'ARM64')]
    [string]$Architecture = 'x64',
    [string]$SdkDir,
    [switch]$SkipTests,
    [switch]$SkipUiTests
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$originalHostArchitecture = $env:PROCESSOR_ARCHITECTURE
if ($env:PROCESSOR_ARCHITEW6432) { $originalHostArchitecture = $env:PROCESSOR_ARCHITEW6432 }

function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $Program $($Arguments -join ' ')"
    }
}

try {
    if ([Environment]::OSVersion.Platform -ne [PlatformID]::Win32NT) {
        throw 'Building the browser requires Windows. Portable core tests can be built separately on Linux.'
    }

    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswherePath)) {
        throw 'Visual Studio C++ Build Tools were not found. Install Desktop development with C++, including CMake tools and a Windows SDK. See BUILDING.md.'
    }
    $visualStudioDirectory = & $vswherePath -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $visualStudioDirectory) {
        throw 'No Visual Studio installation with the MSVC x64/x86 toolset was found. See BUILDING.md.'
    }
    $visualStudioDirectory = @($visualStudioDirectory)[0].Trim()
    $developerCommand = Join-Path $visualStudioDirectory 'Common7\Tools\VsDevCmd.bat'

    $cmakeCommand = Get-Command cmake.exe -ErrorAction SilentlyContinue
    $cmakePath = if ($cmakeCommand) { $cmakeCommand.Source } else {
        Join-Path $visualStudioDirectory 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
    }
    $ninjaCommand = Get-Command ninja.exe -ErrorAction SilentlyContinue
    $ninjaPath = if ($ninjaCommand) { $ninjaCommand.Source } else {
        Join-Path $visualStudioDirectory 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
    }
    foreach ($requiredTool in @($developerCommand, $cmakePath, $ninjaPath)) {
        if (-not (Test-Path -LiteralPath $requiredTool -PathType Leaf)) {
            throw "Required C++ build tool missing: $requiredTool. Install the C++ CMake tools component in Visual Studio Installer."
        }
    }

    # Import the real Visual Studio environment; no hard-coded compiler/SDK versions.
    $targetArchitecture = $Architecture.ToLowerInvariant()
    $hostArchitecture = if ($originalHostArchitecture -match '^ARM64$') { 'arm64' } else { 'x64' }
    $environmentCommand = 'call "{0}" -no_logo -arch={1} -host_arch={2} >nul && set' -f $developerCommand, $targetArchitecture, $hostArchitecture
    $environmentLines = & $env:ComSpec /d /c $environmentCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Visual Studio could not configure the $Architecture compiler. Check that the matching MSVC build tools are installed."
    }
    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            [Environment]::SetEnvironmentVariable($line.Substring(0, $separator), $line.Substring($separator + 1), 'Process')
        }
    }

    $restoreScript = Join-Path $PSScriptRoot 'Restore-WebView2.ps1'
    $resolvedSdk = if ($SdkDir) { & $restoreScript -SdkDir $SdkDir } else { & $restoreScript }
    $resolvedSdk = [string]$resolvedSdk
    if (-not $resolvedSdk) { throw 'WebView2 SDK restore did not return a directory.' }

    $buildDirectory = Join-Path $repositoryRoot "build\windows-$Architecture-$Configuration"
    Write-Host "Building WebCom $Architecture ($Configuration)..."
    Invoke-Checked $cmakePath @(
        '-S', $repositoryRoot, '-B', $buildDirectory, '-G', 'Ninja',
        "-DCMAKE_MAKE_PROGRAM=$ninjaPath", "-DCMAKE_BUILD_TYPE=$Configuration",
        "-DWEBVIEW2_SDK_DIR=$resolvedSdk", '-DWEBCOM_BUILD_APP=ON', '-DBUILD_TESTING=ON', "-DWEBCOM_BUILD_UI_TESTS=$(if ($SkipUiTests) { 'OFF' } else { 'ON' })"
    )
    Invoke-Checked $cmakePath @('--build', $buildDirectory, '--parallel')

    $crossCompiled = ($Architecture -eq 'ARM64' -and $originalHostArchitecture -notmatch '^ARM64$')
    if (-not $SkipTests -and -not $crossCompiled) {
        $ctestPath = Join-Path (Split-Path -Parent $cmakePath) 'ctest.exe'
        Invoke-Checked $ctestPath @('--test-dir', $buildDirectory, '--output-on-failure')
    }
    elseif ($crossCompiled -and -not $SkipTests) {
        Write-Warning 'ARM64 cross-build: the portable and native UI tests were compiled but cannot run on this x64 host. Run them on ARM64 Windows before distributing the ARM64 build.'
    }
    else {
        Write-Warning 'Portable and native UI tests skipped by request.'
    }

    $packageDirectoryName = if ($Architecture -eq 'x64') { 'WebCom' } else { 'WebCom-arm64' }
    $packageDirectory = Join-Path $repositoryRoot "dist\$packageDirectoryName"
    New-Item -ItemType Directory -Path $packageDirectory -Force | Out-Null
    foreach ($binaryName in @('WebCom.exe', 'WebView2Loader.dll')) {
        Copy-Item -LiteralPath (Join-Path $buildDirectory $binaryName) -Destination $packageDirectory -Force
    }
    $uiTestBinary = Join-Path $buildDirectory 'WebCom-ui-tests.exe'
    if (Test-Path -LiteralPath $uiTestBinary) { Copy-Item -LiteralPath $uiTestBinary -Destination $packageDirectory -Force }
    foreach ($licenseName in @('LICENSE', 'NOTICE')) {
        $licensePath = Join-Path $resolvedSdk "$licenseName.txt"
        if (Test-Path -LiteralPath $licensePath) {
            Copy-Item -LiteralPath $licensePath -Destination (Join-Path $packageDirectory "$licenseName-WebView2.txt") -Force
        }
    }
    foreach ($brandAsset in @('webcom.ico', 'webcom-icon.png')) {
        $assetPath = Join-Path $repositoryRoot "resources\$brandAsset"
        if (Test-Path -LiteralPath $assetPath) { Copy-Item -LiteralPath $assetPath -Destination $packageDirectory -Force }
    }
    foreach ($documentName in @('README.md', 'BUILDING.md', 'START-HERE.txt', 'THIRD-PARTY-NOTICES.md', 'run.cmd', 'Test-UI.cmd')) {
        $documentPath = Join-Path $repositoryRoot $documentName
        if (Test-Path -LiteralPath $documentPath) {
            Copy-Item -LiteralPath $documentPath -Destination $packageDirectory -Force
        }
    }
    foreach ($scriptName in @('Measure-WebComMemory.ps1', 'Open-RuntimeDownload.ps1')) {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $scriptName) -Destination $packageDirectory -Force
    }
    $docsDirectory = Join-Path $repositoryRoot 'docs'
    if (Test-Path -LiteralPath $docsDirectory) {
        Copy-Item -LiteralPath $docsDirectory -Destination $packageDirectory -Recurse -Force
    }

    # A local MSVC build must not inherit the supplied cross-preview's hashes.
    $builtExecutable = Join-Path $packageDirectory 'WebCom.exe'
    $builtUiTests = Join-Path $packageDirectory 'WebCom-ui-tests.exe'
    $testResult = if ($SkipTests) { 'NOT RUN: skipped by request.' }
                  elseif ($crossCompiled) { 'NOT RUN: target architecture differs from the host.' }
                  elseif ($SkipUiTests) { 'PASSED: CTest completed portable checks; native Windows UI harness was not built.' }
                  else { 'PASSED: CTest completed, including portable checks and the native Windows UI harness.' }
    $buildInfo = @(
        "WebCom for Windows $((Get-Item -LiteralPath $builtExecutable).VersionInfo.ProductVersion)",
        "Build UTC: $([DateTime]::UtcNow.ToString('yyyy-MM-dd HH:mm:ss'))",
        "Target: $Architecture Windows; configuration: $Configuration",
        'Toolchain: installed Visual Studio C++ tools, CMake and Ninja.',
        'WebView2 Runtime: separately installed Microsoft Edge WebView2 Evergreen.',
        "Executable bytes: $((Get-Item -LiteralPath $builtExecutable).Length)",
        "Executable SHA256: $((Get-FileHash -LiteralPath $builtExecutable -Algorithm SHA256).Hash.ToLowerInvariant())",
        $(if (Test-Path -LiteralPath $builtUiTests) { "Native UI harness SHA256: $((Get-FileHash -LiteralPath $builtUiTests -Algorithm SHA256).Hash.ToLowerInvariant())" } else { 'Native UI harness: NOT BUILT' }),
        "Tests: $testResult",
        'Live-website rendering, DPI transitions, screen-reader behavior and RAM benchmarks require separate Windows acceptance testing.',
        'Native UI checks use isolated temporary data and do not start the WebView2 engine.'
    )
    $buildInfo | Set-Content -LiteralPath (Join-Path $packageDirectory 'docs\BUILD-INFO.txt') -Encoding UTF8

    $packageZip = Join-Path $repositoryRoot "dist\WebCom-Windows-$($Architecture.ToLowerInvariant()).zip"
    Compress-Archive -LiteralPath $packageDirectory -DestinationPath $packageZip -Force
    Write-Host ''
    Write-Host "Build complete: $(Join-Path $packageDirectory 'WebCom.exe')"
    Write-Host "Portable package: $packageZip"
    Write-Host 'The Microsoft Edge WebView2 Evergreen Runtime is also required on the computer that runs WebCom.'
}
catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
