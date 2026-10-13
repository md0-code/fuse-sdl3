<#
Install the Windows parser generators used by the Fuse build. Chocolatey's
package downloads the upstream archive at install time, so retry temporary
download failures before failing the workflow.
#>
[CmdletBinding()]
param(
  [ValidateRange(1, 10)][int]$Attempts = 4,
  [ValidateRange(0, 300)][int]$RetryDelaySeconds = 15
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false

if ((Get-Command win_flex -ErrorAction SilentlyContinue) -and
    (Get-Command win_bison -ErrorAction SilentlyContinue)) {
  Write-Host 'WinFlexBison is already available.'
  return
}

for ($attempt = 1; $attempt -le $Attempts; $attempt++) {
  Write-Host "Installing WinFlexBison (attempt $attempt of $Attempts)"
  & choco install winflexbison3 --yes --no-progress
  $installExitCode = $LASTEXITCODE

  if ($installExitCode -eq 0) {
    if (-not (Get-Command win_flex -ErrorAction SilentlyContinue) -or
        -not (Get-Command win_bison -ErrorAction SilentlyContinue)) {
      throw 'Chocolatey reported success, but win_flex or win_bison is unavailable.'
    }
    Write-Host 'WinFlexBison is ready.'
    return
  }

  if ($attempt -eq $Attempts) {
    throw "WinFlexBison installation failed after $Attempts attempts (Chocolatey exit $installExitCode)."
  }

  Write-Warning "WinFlexBison download failed (Chocolatey exit $installExitCode); retrying."
  Start-Sleep -Seconds ($RetryDelaySeconds * $attempt)
}
