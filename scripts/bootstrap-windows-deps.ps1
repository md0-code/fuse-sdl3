param(
  [string]$Triplet = "x64-windows",
  [string]$VcpkgRoot = "external/vcpkg",
  [string]$OverlayPorts = "vcpkg-ports"
)

$ErrorActionPreference = "Stop"

function Install-WingetPackage {
  param(
    [string]$CommandName,
    [string]$PackageId
  )

  if (Get-Command $CommandName -ErrorAction SilentlyContinue) {
    Write-Host "$CommandName already available"
    return
  }

  Write-Host "Installing $PackageId"
  winget install --id $PackageId -e --accept-package-agreements --accept-source-agreements --silent
}

function Install-WingetPackageForCommands {
  param(
    [string[]]$CommandNames,
    [string]$PackageId
  )

  $allPresent = $true
  foreach ($commandName in $CommandNames) {
    if (-not (Get-Command $commandName -ErrorAction SilentlyContinue)) {
      $allPresent = $false
      break
    }
  }

  if ($allPresent) {
    Write-Host ("{0} already available" -f ($CommandNames -join ", "))
    return
  }

  Write-Host "Installing $PackageId"
  winget install --id $PackageId -e --accept-package-agreements --accept-source-agreements --silent
}

Install-WingetPackage -CommandName perl -PackageId StrawberryPerl.StrawberryPerl
Install-WingetPackage -CommandName pkg-config -PackageId bloodrock.pkg-config-lite
Install-WingetPackage -CommandName ninja -PackageId Ninja-build.Ninja
Install-WingetPackageForCommands -CommandNames @("win_flex", "win_bison") -PackageId WinFlexBison.win_flex_bison

if (-not (Test-Path $VcpkgRoot)) {
  git clone https://github.com/microsoft/vcpkg.git $VcpkgRoot
}

if (-not (Test-Path "external/libspectrum/.git")) {
  git clone --depth 1 https://github.com/speccytools/libspectrum.git external/libspectrum
}

& "$VcpkgRoot/bootstrap-vcpkg.bat"

$installArgs = @(
  "install"
  "--classic"
  "--triplet", $Triplet
  "--overlay-ports", (Resolve-Path $OverlayPorts)
  "sdl3"
  "libxml2"
  "libpng"
  "zlib"
  "bzip2"
  "glib"
  "libspectrum"
)

& "$VcpkgRoot/vcpkg.exe" @installArgs

Write-Host ""
Write-Host "Installed host tools and vcpkg packages for $Triplet."
Write-Host "Native libspectrum is installed into:"
Write-Host "  $((Resolve-Path "$VcpkgRoot/installed/$Triplet").Path)"
Write-Host "Use that prefix with CMake if auto-discovery does not pick it up:"
Write-Host "  -DFUSE_LIBSPECTRUM_ROOT=$((Resolve-Path "$VcpkgRoot/installed/$Triplet").Path.Replace('\','/'))"