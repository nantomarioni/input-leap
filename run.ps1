param(
    [Parameter(Position=0)]
    [ValidateSet('Release','Debug')]
    [string]$Config = 'Debug'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Info  { param([string]$m) Write-Host $m -ForegroundColor Cyan }
function Warn  { param([string]$m) Write-Host $m -ForegroundColor Yellow }
function Error { param([string]$m) Write-Host $m -ForegroundColor Red }
function Good  { param([string]$m) Write-Host $m -ForegroundColor Green }

function Get-BuildDir {
    param([string]$Config)
    $dir = "build/bin/$Config"
    if (Test-Path $dir) { return $dir }
    throw "No build output found for configuration '$Config'. Run .\build_env.ps1 build $Config first."
}

function Stop-ProcessIfRunning([string]$name) {
    $p = Get-Process -Name $name -ErrorAction SilentlyContinue
    if ($p) {
    $count = @($p).Count
    Warn "Stopping existing $name (count=$count)"
    @($p) | Stop-Process -Force
    }
}

try {
    $build = Get-BuildDir -Config $Config
    Info "Using build config '$Config' at: $build"
    $daemon = Join-Path $build 'input-leapd.exe'
    $gui    = Join-Path $build 'input-leap.exe'
    foreach ($f in @($daemon,$gui)) { if (-not (Test-Path $f)) { throw "Missing $f" } }

    Stop-ProcessIfRunning 'input-leapd'
    Stop-ProcessIfRunning 'input-leap'

    $qtDeploy = 'build/qtDeploy'
    if (Test-Path $qtDeploy) { $env:QT_PLUGIN_PATH = (Resolve-Path $qtDeploy).Path }

    Info 'Starting daemon (-f)...'
    $d = Start-Process -FilePath $daemon -ArgumentList '-f' -PassThru
    Start-Sleep -Milliseconds 800
    if ($d.HasExited) { Warn "Daemon exited (code $($d.ExitCode))" } else { Good "Daemon PID $($d.Id)" }

    Info 'Starting GUI...'
    $g = Start-Process -FilePath $gui -PassThru
    Good "GUI PID $($g.Id)"
}
catch {
    Error $_.Exception.Message
    exit 1
}
