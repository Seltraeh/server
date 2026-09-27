[CmdletBinding()]
param(
    [string]$VcpkgRoot = $env:VCPKG_ROOT,
    [string]$ContentRoot,
    [ValidateSet('Separate','None')][string]$ContentMode = 'Separate',
    [ValidateRange(1,16)][int]$Jobs = 2,
    [string]$OutputRoot,
    [string]$InstalledDependencies
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = Split-Path $PSScriptRoot -Parent
if (!$OutputRoot) { $OutputRoot = Join-Path $root 'out\releases' }
if (!$ContentRoot) { $ContentRoot = Join-Path $root 'deploy\game_content' }
if (!$VcpkgRoot) {
    $sibling = Join-Path (Split-Path $root -Parent) 'vcpkg'
    if (Test-Path -LiteralPath (Join-Path $sibling 'vcpkg.exe')) { $VcpkgRoot = $sibling }
}
function Invoke-Checked([string]$Command, [string[]]$Arguments) {
    & $Command @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Command failed with exit code $LASTEXITCODE" }
}
function Get-Revision([string]$Directory) {
    $revision = & git -C $Directory rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw "Not a Git checkout: $Directory" }
    $changes = @(& git -C $Directory status --porcelain --untracked-files=normal)
    if ($LASTEXITCODE -ne 0) { throw "Could not inspect Git status: $Directory" }
    return @{ revision = $revision; dirty = ($changes.Count -gt 0); changes = $changes }
}
function Get-SourceSnapshot {
    $inputs = @{}
    foreach ($repository in @(@{directory=$root; prefix=''}, @{directory=(Join-Path $root 'packet-generator'); prefix='packet-generator/'})) {
        $paths = @(& git -c core.quotepath=false -C $repository.directory ls-files --cached --others --exclude-standard)
        if ($LASTEXITCODE -ne 0) { throw 'Could not inventory release sources.' }
        foreach ($relative in $paths) {
            if ($relative -notmatch '(?i)\.(cpp|hpp|h|inl|kdl|rs|toml|lock|json|cmake|ps1|bat|txt|md|yml)$') { continue }
            $path = Join-Path $repository.directory $relative
            if ([IO.File]::Exists($path)) { $inputs[$repository.prefix + $relative] = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash }
        }
    }
    $ordered = [ordered]@{}
    foreach ($key in @($inputs.Keys | Sort-Object)) { $ordered[$key] = $inputs[$key] }
    return ($ordered | ConvertTo-Json -Compress)
}
function Write-Json($Object, [string]$Path) {
    [IO.File]::WriteAllText($Path, ($Object | ConvertTo-Json -Depth 10), (New-Object Text.UTF8Encoding $false))
}
function New-Zip([string]$Path, [string]$Base, $Files) {
    $zip = [IO.Compression.ZipFile]::Open($Path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $Files) {
            $relative = $file.FullName.Substring($Base.Length).TrimStart('\','/').Replace('\','/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $relative, [IO.Compression.CompressionLevel]::Fastest) | Out-Null
        }
    } finally { $zip.Dispose() }
    if ((Get-Item -LiteralPath $Path).Length -ge 2GB) { throw "ZIP exceeds GitHub's per-asset 2 GiB limit: $Path" }
}
function Write-ContentZip([string]$Output, [int]$Part, [string]$Base, $Files) {
    $path = Join-Path $Output ('game-content-{0:D3}.zip' -f $Part)
    $zip = [IO.Compression.ZipFile]::Open($path, [IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($file in $Files) {
            $relative = 'game_content/' + $file.FullName.Substring($Base.Length).TrimStart('\','/').Replace('\','/')
            [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $file.FullName, $relative, [IO.Compression.CompressionLevel]::Fastest) | Out-Null
        }
    } finally { $zip.Dispose() }
    if ((Get-Item -LiteralPath $path).Length -ge 2GB) { throw "Content ZIP too large: $path" }
}
try {
    foreach ($required in @('packet-generator\Cargo.toml','packet-generator\Cargo.lock','packet-generator\assets\runtime\cpp\CMakeLists.txt')) {
        if (!(Test-Path -LiteralPath (Join-Path $root $required))) { throw "Missing $required. Clone with --recurse-submodules, or run: git submodule update --init --recursive" }
    }
    if (!$VcpkgRoot -or !(Test-Path -LiteralPath (Join-Path $VcpkgRoot 'vcpkg.exe'))) { throw 'Set VCPKG_ROOT or pass -VcpkgRoot with a bootstrapped vcpkg checkout.' }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (!$vs) { throw 'Install Visual Studio/Build Tools with Desktop development with C++.' }
    # Import VS's compiler environment into THIS process. This invokes no filesystem mutations.
    $devcmd = Join-Path $vs 'Common7\Tools\VsDevCmd.bat'
    $environment = & $env:ComSpec /d /c "call `"$devcmd`" -arch=amd64 -host_arch=amd64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Could not initialize the x64 compiler environment.' }
    foreach ($entry in $environment) {
        if ($entry -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process') }
    }
    $env:VCPKG_ROOT = (Resolve-Path -LiteralPath $VcpkgRoot).Path
    $env:VCPKG_MAX_CONCURRENCY = "$Jobs"
    $env:CARGO_BUILD_JOBS = "$Jobs"
    foreach ($command in @('cmake','cargo','git')) { Get-Command $command -ErrorAction Stop | Out-Null }
    if (!(Test-Path -LiteralPath (Join-Path $ContentRoot 'content'))) { throw "ContentRoot must contain the populated content folder: $ContentRoot" }
    $contentBase = (Resolve-Path -LiteralPath $ContentRoot).Path
    $contentFiles = @(Get-ChildItem -LiteralPath $ContentRoot -Recurse -File | Where-Object {
        $relative = $_.FullName.Substring($contentBase.Length).TrimStart('\')
        ($relative -match '^(content|mst)\\') -and ($_.Name -notmatch '(?i)(\.bak(?:-|$)|\.sqlite|\.log$|\.tmp$)')
    } | Sort-Object FullName)
    if ($contentFiles.Count -eq 0) { throw 'No game assets found; refusing to label an empty package playable.' }
    $serverRevision = Get-Revision $root
    $packetRevision = Get-Revision (Join-Path $root 'packet-generator')
    if ($serverRevision.dirty -or $packetRevision.dirty) { Write-Warning 'Packaging current uncommitted work. Commit/push BOTH repositories and the submodule pointer before publishing matching source.' }

    # Required gameplay caches now live in tracked ServerCache sources.
    $localExtensions = @()

    # A scalar pool entry parses as JSON but prevents the whole gacha archive
    # from loading into the generated GachaPull schema.
    $gates = Get-Content -LiteralPath (Join-Path $root 'deploy\archive\gacha.json') -Raw | ConvertFrom-Json
    foreach ($gate in $gates) {
        foreach ($pull in $gate.pool) {
            foreach ($field in @('unit_id','weight')) {
                if ($null -eq $pull -or $pull -isnot [PSCustomObject] -or !($pull.PSObject.Properties.Name -contains $field) -or [string]$pull.$field -notmatch '^\d+$' -or [decimal]$pull.$field -gt [uint32]::MaxValue) {
                    throw "Gacha gate $($gate.id) has an invalid pool entry. Use objects with uint32 unit_id and weight, not bare unit IDs."
                }
            }
        }
    }
    $sourceSnapshot = Get-SourceSnapshot
    Push-Location $root
    try {
        $configure = @('--preset','standalone-release-win64')
        $dependencyDirectory = Join-Path $root 'out\build\standalone-release-win64\vcpkg_installed'
        if ($InstalledDependencies) { $dependencyDirectory = (Resolve-Path -LiteralPath $InstalledDependencies).Path }
        # Always set the default explicitly, so an earlier override cannot keep
        # a normal release build attached to another checkout's dependency tree.
        $configure += "-DVCPKG_INSTALLED_DIR=$dependencyDirectory"
        Invoke-Checked 'cmake' $configure
        Invoke-Checked 'cmake' @('--build','--preset','standalone-release-win64','--parallel',"$Jobs")
    } finally { Pop-Location }

    if ((Get-SourceSnapshot) -cne $sourceSnapshot) {
        throw 'Source/data files changed during compilation. No package was created. Finish other edits or build from a fixed checkout, then rerun.'
    }

    # Every invocation creates a new output directory. Never delete/reuse a user's prior release/save.
    $name = 'bf-server-' + (Get-Date -Format 'yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,6)
    $output = [IO.Path]::GetFullPath((Join-Path $OutputRoot $name))
    $stage = Join-Path $output 'server'
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    Invoke-Checked 'cmake' @('--install',(Join-Path $root 'out\build\standalone-release-win64'),'--config','Release','--prefix',$stage)
    foreach ($folder in @('archive','mst','system')) {
        if (@(Get-ChildItem -LiteralPath (Join-Path $stage $folder) -Filter '*.json').Count -eq 0) { throw "No $folder data installed." }
    }
    $metadata = @{
        created_utc = [DateTime]::UtcNow.ToString('o'); configuration = 'Release'; architecture = 'x64'
        server = $serverRevision; packet_generator = $packetRevision
        client = 'Patched APPX 2.19.6.0, HTTP 127.0.0.1:9960; loopback exemption required'
        local_cache_extensions = $localExtensions
        content_mode = $ContentMode; content_file_count = $contentFiles.Count
    }
    Write-Json $metadata (Join-Path $stage 'release-build.json')
    [IO.File]::WriteAllText((Join-Path $stage 'source-inputs.json'), $sourceSnapshot, (New-Object Text.UTF8Encoding $false))
    $contentBase = (Resolve-Path -LiteralPath $ContentRoot).Path
    $contentIndex = @($contentFiles | ForEach-Object {
        @{path = 'game_content/' + $_.FullName.Substring($contentBase.Length).TrimStart('\','/').Replace('\','/'); size = $_.Length}
    })
    Write-Json $contentIndex (Join-Path $stage 'content-files.json')
    $manifest = @{files = @(Get-ChildItem -LiteralPath $stage -Recurse -File | Sort-Object FullName | ForEach-Object {
        @{path = $_.FullName.Substring($stage.Length).TrimStart('\','/').Replace('\','/'); sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}
    })}
    Write-Json $manifest (Join-Path $stage 'package-manifest.json')
    if (@(Get-ChildItem -LiteralPath $stage -Recurse -File | Where-Object { $_.Name -match '(?i)\.sqlite|\.bak|\.pdb$|^DebugCli' }).Count) { throw 'Private state/debug artifacts appeared in the staging directory.' }
    Add-Type -AssemblyName System.IO.Compression
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    New-Zip (Join-Path $output 'server.zip') $stage @(Get-ChildItem -LiteralPath $stage -Recurse -File)

    if ($ContentMode -eq 'Separate') {
        # Each independent ZIP extracts into the SAME server folder. 1 GiB input bounds
        # keep every asset below GitHub's 2 GiB limit even for incompressible content.
        $batch = New-Object 'Collections.Generic.List[IO.FileInfo]'
        [long]$bytes = 0; $part = 1
        foreach ($file in $contentFiles) {
            if ($file.Length -gt 1GB) { throw "Single asset exceeds content part budget: $($file.Name)" }
            if ($bytes + $file.Length -gt 1GB -and $batch.Count) {
                Write-Host "Writing game-content part $part ..."
                # Use explicit game_content prefix; the source folder may have any name.
                Write-ContentZip $output $part $contentBase $batch
                $batch.Clear(); $bytes = 0; $part++
            }
            $batch.Add($file); $bytes += $file.Length
        }
        if ($batch.Count) { Write-ContentZip $output $part $contentBase $batch }
    }
    Get-ChildItem -LiteralPath $output -Filter '*.zip' | Sort-Object Name | ForEach-Object {
        "{0}  {1}" -f (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant(), $_.Name
    } | Set-Content -LiteralPath (Join-Path $output 'SHA256SUMS.txt') -Encoding ASCII
    Write-Host "Release files: $output"
    Write-Host 'Publish the ZIPs and SHA256SUMS.txt only after the extracted-package smoke test. Nothing was uploaded.'
    if ($ContentMode -eq 'None') { Write-Warning 'Assets were NOT packaged. Players must supply the exact game_content tree recorded in content-files.json.' }
} catch { Write-Error $_; exit 1 }
