[CmdletBinding()]
param(
  [string]$RetroArchRoot = "D:\oldgames\frontends\retroarch",
  [string]$BuildDir = "build-win",
  [string]$ContentPath,
  [int]$MaxFrames = 300
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Resolve-RepoPath {
  param([string]$Path)

  if ([System.IO.Path]::IsPathRooted($Path)) {
    return [System.IO.Path]::GetFullPath($Path)
  }

  return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
}

$buildDirPath = Resolve-RepoPath $BuildDir
$retroArchPath = Resolve-RepoPath $RetroArchRoot
$retroArchExe = Join-Path $retroArchPath "retroarch.exe"
$corePath = Join-Path $buildDirPath "fuse_libretro.dll"
$romDir = Join-Path $buildDirPath "roms"
$libDir = Join-Path $buildDirPath "lib"
$smokeDir = Join-Path $buildDirPath "libretro-smoke"
$defaultContent = Join-Path $smokeDir "if2-loop.rom"
$logPath = Join-Path $smokeDir "retroarch-smoke.log"
$screenshotPath = Join-Path $smokeDir "retroarch-smoke.png"

if (-not (Test-Path $retroArchExe)) {
  throw "RetroArch executable not found: $retroArchExe"
}

if (-not (Test-Path $corePath)) {
  throw "Libretro core not found: $corePath"
}

if (-not (Test-Path $romDir) -or -not (Test-Path $libDir)) {
  throw "Missing staged ROM or asset directories in $buildDirPath. Build fuse-libretro first."
}

New-Item -ItemType Directory -Force -Path $smokeDir | Out-Null

if (-not $ContentPath) {
  $bytes = New-Object byte[] 16384
  $bytes[0] = 0x18
  $bytes[1] = 0xFE
  [System.IO.File]::WriteAllBytes($defaultContent, $bytes)
  $ContentPath = $defaultContent
}
else {
  $ContentPath = Resolve-RepoPath $ContentPath
}

if (-not (Test-Path $ContentPath)) {
  throw "Smoke-test content not found: $ContentPath"
}

Remove-Item $logPath, $screenshotPath -ErrorAction SilentlyContinue

$arguments = @(
  "--verbose",
  "--log-file=$logPath",
  "--max-frames=$MaxFrames",
  "--max-frames-ss",
  "--max-frames-ss-path=$screenshotPath",
  "-L", $corePath,
  $ContentPath
)

Write-Host "Running RetroArch smoke test..."
Write-Host "  retroarch: $retroArchExe"
Write-Host "  core:      $corePath"
Write-Host "  content:   $ContentPath"
Write-Host "  frames:    $MaxFrames"

$process = Start-Process -FilePath $retroArchExe -ArgumentList $arguments -WorkingDirectory $buildDirPath -PassThru -Wait

if ($process.ExitCode -ne 0) {
  if (Test-Path $logPath) { Get-Content $logPath }
  throw "RetroArch smoke test failed with exit code $($process.ExitCode)"
}

if (-not (Test-Path $screenshotPath)) {
  if (Test-Path $logPath) { Get-Content $logPath }
  throw "RetroArch smoke test did not produce the expected screenshot: $screenshotPath"
}

if (Test-Path $logPath) {
  $logText = [System.IO.File]::ReadAllText($logPath)
  if ($logText -match 'libretro: failed|Failed to load content|error initialising|couldn''t open') {
    Get-Content $logPath
    throw "RetroArch smoke test log contains a core load/runtime failure"
  }
}

Write-Host "RetroArch smoke test passed"
Write-Host "  log:        $logPath"
Write-Host "  screenshot: $screenshotPath"