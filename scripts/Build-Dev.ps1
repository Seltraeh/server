[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')][string]$Configuration = 'Debug',
    [ValidateRange(1,32)][int]$Jobs = 2,
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [switch]$BootstrapVcpkg,
    [switch]$ConfigureOnly,
    [switch]$Fresh,
    [string]$BuildDirectory,
    [string]$InstalledDependencies
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE" }
}
try {
    if ($env:OS -ne 'Windows_NT') { throw 'Use the debug-lnx64 CMake preset on Linux.' }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (!(Test-Path -LiteralPath $vswhere)) { throw 'Install Visual Studio with Desktop development with C++, including CMake tools and a Windows SDK.' }
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'No Visual Studio C++ tools found. Add Desktop development with C++ in Visual Studio Installer.' }
    $devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
    $environment = & $env:ComSpec /d /c "call `"$devcmd`" -no_logo -arch=amd64 -host_arch=amd64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the Visual Studio x64 environment.' }
    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
    }
    $ninjaDirectory = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
    if (!(Get-Command ninja -ErrorAction SilentlyContinue) -and (Test-Path -LiteralPath (Join-Path $ninjaDirectory 'ninja.exe'))) {
        $env:PATH = "$ninjaDirectory;$env:PATH"
    }
    foreach ($command in @('git','cmake','ninja','cargo','cl')) {
        if (!(Get-Command $command -ErrorAction SilentlyContinue)) { throw "Missing $command. See docs/DEVELOPMENT.md for prerequisites." }
    }
    # VS may prepend its older bundled CMake ahead of a separately installed 4.x.
    $cmake = $null
    foreach ($candidate in @(Get-Command cmake -All)) {
        $cmakeVersion = (& $candidate.Source --version | Select-Object -First 1) -replace '^cmake version ', ''
        if ([version]($cmakeVersion -replace '-.*$', '') -ge [version]'4.0') { $cmake = $candidate.Source; break }
    }
    if (!$cmake) { throw 'CMake 4.0+ is required. Install Kitware.CMake and reopen PowerShell.' }
    if (!(Test-Path -LiteralPath (Join-Path $root 'packet-generator\Cargo.toml'))) {
        Invoke-Checked 'git' @('-C', $root, 'submodule', 'update', '--init', '--recursive')
    }
    # Never update an already initialized submodule behind a contributor's back.
    foreach ($inputFile in @('packet-generator\Cargo.lock','packet-generator\assets\runtime\cpp\CMakeLists.txt','packaging\Install.cmake','packaging\config.json')) {
        if (!(Test-Path -LiteralPath (Join-Path $root $inputFile))) { throw "Required source file missing: $inputFile. Pull the complete source and initialize submodules." }
    }
    if (!$VcpkgRoot) { $VcpkgRoot = Join-Path $root '.tools\vcpkg' }
    if (!(Test-Path -LiteralPath (Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'))) {
        if (!$BootstrapVcpkg) { throw 'Set VCPKG_ROOT, pass -VcpkgRoot, or use -BootstrapVcpkg -VcpkgRoot .tools\vcpkg to install a local copy.' }
        if (Test-Path -LiteralPath $VcpkgRoot) { throw "Not a vcpkg checkout: $VcpkgRoot. Choose a new directory; existing files will not be replaced." }
        Invoke-Checked 'git' @('clone', 'https://github.com/microsoft/vcpkg.git', $VcpkgRoot)
        $baseline = (Get-Content -LiteralPath (Join-Path $root 'vcpkg-configuration.json') -Raw | ConvertFrom-Json).'default-registry'.baseline
        Invoke-Checked 'git' @('-C', $VcpkgRoot, 'checkout', '--detach', $baseline)
    }
    $VcpkgRoot = (Resolve-Path -LiteralPath $VcpkgRoot).Path
    if (!(Test-Path -LiteralPath (Join-Path $VcpkgRoot 'vcpkg.exe'))) {
        Invoke-Checked (Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat') @('-disableMetrics')
    }
    $env:VCPKG_ROOT = $VcpkgRoot
    $env:VCPKG_MAX_CONCURRENCY = "$Jobs"
    $env:CARGO_BUILD_JOBS = "$Jobs"
    if (!$BuildDirectory) { $BuildDirectory = Join-Path $root 'out\build\debug-win64' }
    $BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
    # Building needs no artwork. Do not overwrite an existing config or save.
    $config = Join-Path $root 'deploy\config.json'
    if (!(Test-Path -LiteralPath $config)) {
        Copy-Item -LiteralPath (Join-Path $root 'packaging\config.json') -Destination $config
        Write-Host 'Created deploy/config.json from the shared template.'
    }
    $launchDirectory = Join-Path $root '.vs'
    $launchConfig = Join-Path $launchDirectory 'launch.vs.json'
    if (!(Test-Path -LiteralPath $launchConfig)) {
        New-Item -ItemType Directory -Path $launchDirectory -Force | Out-Null
        Copy-Item -LiteralPath (Join-Path $root 'launch.vs.json') -Destination $launchConfig
    }
    Push-Location $root
    try {
        $configure = @('--preset', 'debug-win64', '-B', $BuildDirectory)
        if ($Fresh) { $configure += '--fresh' }
        if ($InstalledDependencies) {
            $configure += "-DVCPKG_INSTALLED_DIR=$((Resolve-Path -LiteralPath $InstalledDependencies).Path)"
        }
        Invoke-Checked $cmake $configure
        if (!$ConfigureOnly) {
            Invoke-Checked $cmake @('--build', $BuildDirectory, '--config', $Configuration, '--target', 'gimuserverw', '--parallel', "$Jobs")
            Write-Host "Built: $BuildDirectory\standalone_frontend\$Configuration\gimuserverw.exe"
        }
    } finally { Pop-Location }
} catch {
    Write-Error $_ -ErrorAction Continue
    exit 1
}
