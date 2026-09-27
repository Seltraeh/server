$ErrorActionPreference = 'Stop'
try {
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
    if (!$isAdmin) { throw 'Run PowerShell as administrator, then run this script. Only this one-time client setup needs administrator access.' }
    $packages = @(Get-AppxPackage -Name 'gumi.BraveFrontier')
    if ($packages.Count -ne 1) { throw 'Expected one installed gumi.BraveFrontier package for this Windows user. Install the patched client first.' }
    & CheckNetIsolation.exe LoopbackExempt -a "-n=$($packages[0].PackageFamilyName)"
    if ($LASTEXITCODE -ne 0) { throw 'Windows could not enable the client loopback exemption.' }
    Write-Host 'Brave Frontier can now connect to the local server.'
} catch { Write-Error $_; exit 1 }
