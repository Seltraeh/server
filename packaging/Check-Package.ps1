param([switch]$VerifyHashes, [switch]$SkipPortCheck)
$ErrorActionPreference = 'Stop'
try {
    $root = $PSScriptRoot
    $manifest = Get-Content -LiteralPath (Join-Path $root 'package-manifest.json') -Raw | ConvertFrom-Json
    foreach ($file in $manifest.files) {
        $path = Join-Path $root $file.path
        if (!(Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing package file: $($file.path). Extract server.zip again." }
        if ($VerifyHashes -and $file.path -ne 'config.json' -and (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.sha256) {
            throw "Package file differs from this release: $($file.path)"
        }
    }
    $assetIndex = Get-Content -LiteralPath (Join-Path $root 'content-files.json') -Raw | ConvertFrom-Json
    foreach ($entry in $assetIndex) {
        $path = Join-Path $root $entry.path
        if (![IO.File]::Exists($path)) { throw "Missing game asset: $($entry.path). Extract ALL matching game-content ZIPs into this folder, then retry." }
        if ((New-Object IO.FileInfo $path).Length -ne $entry.size) { throw "Incomplete/wrong-version game asset: $($entry.path)" }
    }
    if (!$assetIndex -or $assetIndex.Count -lt 1) { throw 'This release has no game asset inventory. Rebuild with a populated ContentRoot.' }
    $config = Get-Content -LiteralPath (Join-Path $root 'config.json') -Raw | ConvertFrom-Json
    if (!$SkipPortCheck) {
        $port = [int]$config.listeners[0].port
        $socket = New-Object Net.Sockets.TcpClient
        try {
            $attempt = $socket.BeginConnect('127.0.0.1', $port, $null, $null)
            if ($attempt.AsyncWaitHandle.WaitOne(250)) {
                try { $socket.EndConnect($attempt) } catch { }
                if ($socket.Connected) { throw "Port $port is already in use. Close the other server instance first." }
            }
        } finally { $socket.Close() }
    }
    Write-Host 'Package files and game assets are present.'
    exit 0
} catch {
    Write-Host $_.Exception.Message -ForegroundColor Red
    exit 1
}
