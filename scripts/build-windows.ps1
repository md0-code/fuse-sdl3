param(
  [string]$BuildDir = "build-win-native",
  [string]$Triplet = "x64-windows",
  [switch]$Package
)

$ErrorActionPreference = "Stop"

$buildType = ""
if ($Package) {
  $buildType = "Release"
}

$buildScript = Join-Path $PSScriptRoot "build-native-win.cmd"

& "$PSScriptRoot/bootstrap-windows-deps.ps1" -Triplet $Triplet
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

$buildDirPath = Join-Path (Resolve-Path "$PSScriptRoot/..").Path $BuildDir
if ($buildType) {
  & $buildScript $buildDirPath $Triplet $buildType
} else {
  & $buildScript $buildDirPath $Triplet
}
if ($LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}

$exePath = Join-Path $buildDirPath "fuse.exe"
if (-not (Test-Path $exePath)) {
  Write-Error "Expected build output not found: $exePath"
  exit 1
}

$smokeStdout = Join-Path $buildDirPath "smoke-output.txt"
$smokeStderr = Join-Path $buildDirPath "smoke-error.txt"
Remove-Item $smokeStdout, $smokeStderr -ErrorAction SilentlyContinue

$process = Start-Process -FilePath $exePath -ArgumentList '-V' `
  -RedirectStandardOutput $smokeStdout -RedirectStandardError $smokeStderr `
  -PassThru -Wait

if ($process.ExitCode -ne 0) {
  if (Test-Path $smokeStdout) { Get-Content $smokeStdout }
  if (Test-Path $smokeStderr) { Get-Content $smokeStderr }
  Write-Error "Runtime smoke test failed with exit code $($process.ExitCode)"
  exit $process.ExitCode
}

Write-Host "Runtime smoke test passed: fuse.exe -V"

if ($Package) {
  & $buildScript $buildDirPath $Triplet $buildType package
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }

  Write-Host "Package archives generated in $buildDirPath"
}

exit 0