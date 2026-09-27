# Run against a freshly extracted package, never a player's existing server folder.
param([Parameter(Mandatory=$true)][string]$PackageDirectory, [int]$Port = 19961)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$folder = (Resolve-Path -LiteralPath $PackageDirectory).Path
$configPath = Join-Path $folder 'config.json'
$original = [IO.File]::ReadAllText($configPath)
$process = $null
$oldPath = $env:PATH
function Assert($Condition, [string]$Message) { if (!$Condition) { throw $Message } }
function Protect-Body([string]$Text, [string]$Key, [bool]$Decrypt = $false) {
    $aes = [Security.Cryptography.Aes]::Create()
    try {
        $keyBytes = New-Object byte[] 16
        $source = [Text.Encoding]::UTF8.GetBytes($Key)
        [Array]::Copy($source, $keyBytes, [Math]::Min(16,$source.Length))
        $aes.Key = $keyBytes; $aes.Mode = 'ECB'; $aes.Padding = 'PKCS7'
        if ($Decrypt) {
            $bytes = [Convert]::FromBase64String($Text)
            return [Text.Encoding]::UTF8.GetString($aes.CreateDecryptor().TransformFinalBlock($bytes,0,$bytes.Length))
        }
        $bytes = [Text.Encoding]::UTF8.GetBytes($Text)
        return [Convert]::ToBase64String($aes.CreateEncryptor().TransformFinalBlock($bytes,0,$bytes.Length))
    } finally { $aes.Dispose() }
}
function Request-Gme([string]$Group, [string]$Key, $Body) {
    $outer = @{
        F4q6i9xe = @{ Hhgi79M1 = $Group; aV6cLn3v = 'release-smoke' }
        a3vSYuq2 = @{ Kn51uR4Y = (Protect-Body ($Body | ConvertTo-Json -Depth 20 -Compress) $Key) }
    }
    $answer = Invoke-RestMethod -UseBasicParsing -Uri "$base/bf/gme/action.php" -Method Post -ContentType 'application/json' -Body ($outer | ConvertTo-Json -Depth 10 -Compress) -TimeoutSec 45
    Assert (!$answer.b5PH6mZa) "GME request $Group returned an error: $($answer | ConvertTo-Json -Compress -Depth 5)"
    return (Protect-Body $answer.a3vSYuq2.Kn51uR4Y $Key $true | ConvertFrom-Json)
}
try {
    Assert (!(Test-Path -LiteralPath (Join-Path $folder 'gme.sqlite'))) 'Test requires a fresh extraction with NO save. It will create a disposable test save.'
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $folder 'Check-Package.ps1') -VerifyHashes -SkipPortCheck
    Assert ($LASTEXITCODE -eq 0) 'Package verification failed.'
    $probe = New-Object Net.Sockets.TcpClient
    try { $probe.Connect('127.0.0.1',$Port) } catch { }
    $occupied = $probe.Connected; $probe.Close()
    Assert (!$occupied) "Test port $Port is already occupied."
    $config = $original | ConvertFrom-Json
    $config.listeners[0].port = $Port
    [IO.File]::WriteAllText($configPath, ($config | ConvertTo-Json -Depth 20), (New-Object Text.UTF8Encoding $false))
    # No Rust/VS/vcpkg/dev DLL paths. Launch with no config argument from another cwd.
    $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
    $process = Start-Process -FilePath (Join-Path $folder 'gimuserverw.exe') -WorkingDirectory $env:TEMP -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $folder 'smoke-stdout.log') -RedirectStandardError (Join-Path $folder 'smoke-stderr.log')
    $base = "http://127.0.0.1:$Port"
    $ready = $false
    for ($i=0; $i -lt 60; $i++) {
        if ($process.HasExited) { throw "Packaged server exited with $($process.ExitCode); inspect smoke logs." }
        try { $cap = Invoke-RestMethod -UseBasicParsing "$base/offline_mod/fps_cap" -TimeoutSec 1; $ready=$true; break } catch { Start-Sleep -Milliseconds 500 }
    }
    Assert $ready 'Packaged server did not become ready.'
    Assert ([int]$cap -eq 60) 'Proxy FPS endpoint failed.'
    $features = Invoke-RestMethod -UseBasicParsing "$base/bf/gme/featureCheck.php" -TimeoutSec 45
    Assert ($null -ne $features) 'Feature config failed.'
    $guest = Invoke-RestMethod -UseBasicParsing "$base/accounts/guest/login/" -TimeoutSec 45
    Assert ($guest.status_no -eq '0' -and $guest.game_user_id) 'Fresh database/guest login failed.'
    $identity = @{ IKqx1Cn9 = @(@{ iN7buP2h = $guest.game_user_id }) }
    $initialize = Request-Gme 'MfZyu1q9' 'EmcshnQoDr20TZz1' $identity
    Assert ($null -ne $initialize.IKqx1Cn9) 'Encrypted Initialize response missing login info.'
    $baseline = Get-Content -LiteralPath (Join-Path $folder 'mst\version_info_mst.json') -Raw | ConvertFrom-Json
    foreach ($row in $baseline.KeC10fuL) {
        $offered = @($initialize.KeC10fuL | Where-Object { $_.moWQ30GH -eq $row.moWQ30GH })
        Assert ($offered.Count -eq 1 -and [int]$offered[0].d2RFtP8T -ge [int]$row.d2RFtP8T) "Initialize lost/regressed MST table $($row.moWQ30GH)."
    }
    $identity.IKqx1Cn9[0].B5JQyV8j = 'ReleaseTest'
    $create = @{ IKqx1Cn9 = $identity.IKqx1Cn9; '9u45RGcV' = @(@{Kn51uR4Y='1'}) }
    $created = Request-Gme 'uV6yH5MX' '4agnATy2DrJsWzQk' $create
    $user = Request-Gme 'cTZ3W2JG' 'ScJx6ywWEb0A3njT' $identity
    Assert ($null -ne $user.IKqx1Cn9 -and $user.IKqx1Cn9[0].h7eY3sAK) 'Fresh game user could not be loaded.'
    Assert (@($user.'4ceMWH6k').Count -ge 3) 'Starter unit inventory was not provisioned from the packaged archive.'
    $asset = Get-ChildItem -LiteralPath (Join-Path $folder 'game_content\content') -Recurse -File -Filter '*.png' | Select-Object -First 1
    Assert ($null -ne $asset) 'No representative downloadable PNG found.'
    $urlPath = $asset.FullName.Substring((Join-Path $folder 'game_content').Length).Replace('\','/')
    $download = Join-Path $folder 'smoke-asset.tmp'
    Invoke-WebRequest -UseBasicParsing -Uri ($base+$urlPath) -OutFile $download -TimeoutSec 45
    Assert ((Get-FileHash -LiteralPath $asset.FullName).Hash -eq (Get-FileHash -LiteralPath $download).Hash) 'Asset response differs from packaged bytes.'
    $mstParts = @()
    $flatMst = Join-Path $folder 'game_content\mst'
    if (Test-Path -LiteralPath $flatMst) {
        $mstParts += @(Get-ChildItem -LiteralPath $flatMst -Filter '*.dat' -File)
    }
    foreach ($manifestPath in @(Get-ChildItem -LiteralPath (Join-Path $folder 'game_content\content') -Filter manifest.json -Recurse -File)) {
        $manifest = Get-Content -LiteralPath $manifestPath.FullName -Raw | ConvertFrom-Json
        if (!$manifest.file_key -or !$manifest.file_count) { continue }
        $parts = @(Get-ChildItem -LiteralPath $manifestPath.DirectoryName -Filter '*.dat' -File)
        Assert ($parts.Count -eq [int]$manifest.file_count) "Incomplete staged MST table: $($manifest.table)"
        $offered = @($initialize.KeC10fuL | Where-Object { $_.moWQ30GH -eq $manifest.table })
        Assert ($offered.Count -eq 1 -and [int]$offered[0].d2RFtP8T -eq [int]$manifest.version) "Staged MST table not offered: $($manifest.table)"
        $mstParts += $parts
    }
    foreach ($part in $mstParts) {
        Invoke-WebRequest -UseBasicParsing -Uri "$base/mst/$($part.Name)" -OutFile $download -TimeoutSec 45
        Assert ((Get-FileHash -LiteralPath $part.FullName).Hash -eq (Get-FileHash -LiteralPath $download).Hash) "MST download failed: $($part.Name)"
    }
    $log = Get-Content -LiteralPath (Join-Path $folder 'smoke-stdout.log') -Raw
    Assert ($log -match 'Loaded \d+ unit archive records') 'Unit archive was not loaded.'
    Assert ($log -notmatch 'Unable to set up|Fatal exception|ERROR') 'Startup emitted errors; inspect smoke-stdout.log.'
    $savedUserId = $user.IKqx1Cn9[0].h7eY3sAK
    Stop-Process -Id $process.Id
    $process.WaitForExit()
    $process = Start-Process -FilePath (Join-Path $folder 'gimuserverw.exe') -WorkingDirectory $env:TEMP -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $folder 'smoke-restart-stdout.log') -RedirectStandardError (Join-Path $folder 'smoke-restart-stderr.log')
    $ready = $false
    for ($i=0; $i -lt 60; $i++) {
        if ($process.HasExited) { throw "Packaged server failed to restart: $($process.ExitCode)" }
        try { $null = Invoke-RestMethod -UseBasicParsing "$base/offline_mod/fps_cap" -TimeoutSec 1; $ready=$true; break } catch { Start-Sleep -Milliseconds 500 }
    }
    Assert $ready 'Packaged server did not restart.'
    $resumedGuest = Invoke-RestMethod -UseBasicParsing "$base/accounts/guest/login/" -TimeoutSec 45
    Assert ($resumedGuest.game_user_id -eq $guest.game_user_id) 'Guest identity changed after restart.'
    $resumed = Request-Gme 'cTZ3W2JG' 'ScJx6ywWEb0A3njT' $identity
    Assert ($resumed.IKqx1Cn9[0].h7eY3sAK -eq $savedUserId -and @($resumed.'4ceMWH6k').Count -eq @($user.'4ceMWH6k').Count) 'Save/user inventory did not persist across restart.'
    $restartLog = Get-Content -LiteralPath (Join-Path $folder 'smoke-restart-stdout.log') -Raw
    Assert ($restartLog -notmatch 'Unable to set up|Fatal exception|ERROR') 'Restart emitted errors; inspect smoke-restart-stdout.log.'
    @{passed=$true; mst_parts_verified=$mstParts.Count; mst_versions_verified=@($baseline.KeC10fuL).Count; server_sha256=(Get-FileHash -LiteralPath (Join-Path $folder 'gimuserverw.exe')).Hash; port=$Port; checks=@('minimal PATH','foreign working directory','fresh database','guest login','encrypted Initialize','CreateUser/starter provisioning','UserInfo','proxy FPS','feature config','asset bytes','available MST parts','save persistence after restart')} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $folder 'smoke-result.json') -Encoding UTF8
    Write-Host 'Extracted Release smoke test passed. Do not distribute this test folder/save; publish the original ZIPs.'
} finally {
    if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id }
    $env:PATH = $oldPath
    [IO.File]::WriteAllText($configPath,$original,(New-Object Text.UTF8Encoding $false))
}
