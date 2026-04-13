<#
.SYNOPSIS
    Builds, installs, or uninstalls Fuse SDL3 on Windows.

.DESCRIPTION
    Configures and builds Fuse SDL3 using CMake, Ninja, and vcpkg.
    Optionally installs or uninstalls via cmake --install.

.PARAMETER BuildDir
    Build output directory relative to the repo root. Defaults to "build-win".

.PARAMETER Triplet
    vcpkg target triplet. Defaults to "x64-windows".

.PARAMETER VcpkgRoot
    Path to the vcpkg checkout. Defaults to "external/vcpkg".

.PARAMETER BinaryCacheDir
    Optional path for the vcpkg binary cache directory.

.PARAMETER Package
  Build a release archive after compiling. With -Libretro, package only the
  libretro DLL and matching .info file.

.PARAMETER Install
    Install after building using cmake --install.

.PARAMETER InstallPrefix
    Installation prefix used with -Install. Defaults to the cmake configured prefix.

.PARAMETER Uninstall
    Uninstall using BUILD_DIR/install_manifest.txt.

.PARAMETER RuntimeSmokeTest
  Run the desktop runtime smoke test after building.

.PARAMETER Libretro
  Build the libretro core as a standalone single-DLL artifact using the
  x64-windows-static vcpkg triplet and a dedicated build directory by default.

.PARAMETER ShaderSmokeTest
    Run a shader smoke test after building.

.PARAMETER BootstrapOnly
    Bootstrap vcpkg and exit without building.

.PARAMETER ConfigureOnly
    Configure CMake and exit without building.

.PARAMETER SkipBootstrap
    Skip bootstrapping vcpkg and host tools.

.PARAMETER RebuildLibspectrum
    Force a rebuild of the bundled libspectrum.

.PARAMETER VerboseVcpkg
    Enable verbose vcpkg output.

.EXAMPLE
    .\scripts\build-win.ps1
    Builds Fuse SDL3 in the default build-win directory.

.EXAMPLE
    .\scripts\build-win.ps1 -Package
    Builds and packages Fuse SDL3 as a portable zip archive.

.EXAMPLE
    .\scripts\build-win.ps1 -Install
    Builds and installs Fuse SDL3 to the default CMake prefix.

.EXAMPLE
    .\scripts\build-win.ps1 -Install -InstallPrefix "C:\Program Files\Fuse"
    Builds and installs Fuse SDL3 to a custom prefix.

.EXAMPLE
    .\scripts\build-win.ps1 -Uninstall
    Uninstalls Fuse SDL3 using the install manifest from the build directory.

.EXAMPLE
  .\scripts\build-win.ps1 -Libretro
  Configures and builds a standalone libretro core in build-win-libretro-standalone.

.EXAMPLE
  .\scripts\build-win.ps1 -Libretro -Package
  Builds the standalone libretro core and packages the DLL and .info file in a zip archive.
#>
[CmdletBinding()]
param(
  [string]$BuildDir = "build-win",
  [string]$Triplet = "x64-windows",
  [string]$VcpkgRoot = "external/vcpkg",
  [string]$BinaryCacheDir,
  [switch]$Package,
  [switch]$Install,
  [string]$InstallPrefix,
  [switch]$Uninstall,
  [switch]$RuntimeSmokeTest,
  [Alias('LibretroStandalone')]
  [switch]$Libretro,
  [switch]$ShaderSmokeTest,
  [switch]$BootstrapOnly,
  [switch]$ConfigureOnly,
  [switch]$SkipBootstrap,
  [switch]$RebuildLibspectrum,
  [switch]$VerboseVcpkg
)

$ErrorActionPreference = "Stop"
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ($Libretro) {
  if (-not $PSBoundParameters.ContainsKey('BuildDir')) {
    $BuildDir = 'build-win-libretro-standalone'
  }

  if (-not $PSBoundParameters.ContainsKey('Triplet')) {
    $Triplet = 'x64-windows-static'
  }

  if ($Install) {
    throw '-Libretro cannot be combined with -Install.'
  }

  if ($ShaderSmokeTest) {
    throw '-Libretro cannot be combined with -ShaderSmokeTest.'
  }

  if ($RuntimeSmokeTest) {
    throw '-Libretro cannot be combined with -RuntimeSmokeTest.'
  }
}

function Resolve-RepoPath {
  param([string]$Path)

  if ([System.IO.Path]::IsPathRooted($Path)) {
    return [System.IO.Path]::GetFullPath($Path)
  }

  return [System.IO.Path]::GetFullPath((Join-Path $RepoRoot $Path))
}

function Get-RepoProjectVersion {
  $cmakeListsPath = Join-Path $RepoRoot 'CMakeLists.txt'
  $cmakeText = [System.IO.File]::ReadAllText($cmakeListsPath)
  $match = [System.Text.RegularExpressions.Regex]::Match(
    $cmakeText,
    'project\s*\(\s*fuse\s+VERSION\s+([0-9]+(?:\.[0-9]+)*)\b'
  )

  if (-not $match.Success) {
    throw "Could not determine the project version from $cmakeListsPath"
  }

  return $match.Groups[1].Value
}

function Get-WindowsPackageArchitectureLabel {
  param([string]$TripletName)

  if ($TripletName -match '^x64-') {
    return 'AMD64'
  }

  if ($TripletName -match '^x86-') {
    return 'x86'
  }

  if ($TripletName -match '^arm64-') {
    return 'ARM64'
  }

  if ($TripletName -match '^arm-') {
    return 'ARM'
  }

  return $env:PROCESSOR_ARCHITECTURE
}

function Set-RepoVcpkgEnvironment {
  param(
    [string]$ResolvedVcpkgRoot,
    [string]$ResolvedBinaryCacheDir
  )

  $env:VCPKG_ROOT = $ResolvedVcpkgRoot

  if ($ResolvedBinaryCacheDir) {
    New-Item -ItemType Directory -Force -Path $ResolvedBinaryCacheDir | Out-Null
    $env:VCPKG_DEFAULT_BINARY_CACHE = $ResolvedBinaryCacheDir
  }
}

function Get-ActiveRepoVcpkgProcesses {
  param([string]$ResolvedVcpkgRoot)

  $normalizedRoot = [System.IO.Path]::GetFullPath($ResolvedVcpkgRoot)

  return Get-CimInstance Win32_Process -Filter "Name = 'vcpkg.exe'" |
    Where-Object {
      $_.CommandLine -and $_.CommandLine.Contains($normalizedRoot)
    }
}

function Assert-NoRepoVcpkgLockOwner {
  param(
    [string]$ResolvedVcpkgRoot,
    [int[]]$IgnoreProcessIds = @()
  )

  $active = Get-ActiveRepoVcpkgProcesses -ResolvedVcpkgRoot $ResolvedVcpkgRoot |
    Where-Object { $IgnoreProcessIds -notcontains $_.ProcessId }

  if ($active) {
    Write-Host "Another repo-local vcpkg process is already running:"
    foreach ($process in $active) {
      Write-Host "  PID $($process.ProcessId): $($process.CommandLine)"
    }

    throw "Refusing to wait on the vcpkg filesystem lock. Stop the existing repo-local vcpkg/CMake process and rerun the script."
  }
}

function Show-VcpkgHints {
  param(
    [string]$ResolvedVcpkgRoot,
    [string]$ResolvedBinaryCacheDir
  )

  if ($VerboseVcpkg) {
    Write-Host "Verbose vcpkg logging enabled."
    Write-Host "vcpkg buildtrees: $(Join-Path $ResolvedVcpkgRoot 'buildtrees')"
    Write-Host "vcpkg installed: $(Join-Path $ResolvedVcpkgRoot 'installed')"
  }

  if ($ResolvedBinaryCacheDir) {
    Write-Host "vcpkg binary cache: $ResolvedBinaryCacheDir"
  }
}

function Show-VcpkgFailureHints {
  param(
    [string]$ResolvedVcpkgRoot,
    [string]$BuildDirectory
  )

  $buildtreesDir = Join-Path $ResolvedVcpkgRoot "buildtrees"
  Write-Host ""
  Write-Host "vcpkg failure hints:"
  Write-Host "  buildtrees: $buildtreesDir"

  if ($BuildDirectory) {
    $manifestLog = Join-Path $BuildDirectory "vcpkg-manifest-install.log"
    if (Test-Path $manifestLog) {
      Write-Host "  manifest log: $manifestLog"
    }
  }

  if (Test-Path $buildtreesDir) {
    $recentLogs = Get-ChildItem -Path $buildtreesDir -Recurse -File -Filter *.log -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTime -Descending |
      Select-Object -First 8

    if ($recentLogs) {
      Write-Host "  recent logs:"
      foreach ($log in $recentLogs) {
        Write-Host "    $($log.FullName)"
      }
    }
  }
}

function Get-VswherePath {
  $programFilesX86 = ${env:ProgramFiles(x86)}
  if (-not $programFilesX86) {
    return $null
  }

  $candidate = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
  if (Test-Path $candidate) {
    return $candidate
  }

  return $null
}

function Get-VsDevCmdPath {
  $installationPath = Get-VsInstallationPath

  $vsDevCmd = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
  if (-not (Test-Path $vsDevCmd)) {
    throw "Found Visual Studio at '$installationPath' but '$vsDevCmd' is missing. Repair the Visual Studio C++ installation and rerun this script."
  }

  return $vsDevCmd
}

function Get-VsInstallationPath {
  $vswhere = Get-VswherePath
  if (-not $vswhere) {
    throw "Could not find vswhere.exe. Install Visual Studio or Build Tools with the Desktop development with C++ workload."
  }

  $installationPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if ($LASTEXITCODE -ne 0) {
    throw "Failed to query Visual Studio installations with vswhere."
  }

  $installationPath = ($installationPath | Select-Object -First 1)
  if ($installationPath) {
    $installationPath = $installationPath.Trim()
  }

  if (-not $installationPath) {
    throw "Could not find a Visual Studio installation with the Desktop development with C++ workload. Install that workload and rerun this script."
  }

  return $installationPath
}

function Import-VsDevEnvironment {
  $installationPath = Get-VsInstallationPath
  $devShellModule = Join-Path $installationPath "Common7\Tools\Microsoft.VisualStudio.DevShell.dll"

  Write-Host "Importing Visual Studio developer environment..."

  if (Test-Path $devShellModule) {
    Import-Module $devShellModule -ErrorAction Stop | Out-Null
    Enter-VsDevShell -VsInstallPath $installationPath -SkipAutomaticLocation -DevCmdArguments "-arch=x64 -host_arch=x64" | Out-Null
  }
  else {
    $vsDevCmd = Get-VsDevCmdPath
    $tempScriptSeed = [System.IO.Path]::GetTempFileName()
    $tempEnvSeed = [System.IO.Path]::GetTempFileName()
    $tempExportSeed = [System.IO.Path]::GetTempFileName()
    $tempScript = [System.IO.Path]::ChangeExtension($tempScriptSeed, ".cmd")
    $tempEnv = [System.IO.Path]::ChangeExtension($tempEnvSeed, ".txt")
    $tempExportScript = [System.IO.Path]::ChangeExtension($tempExportSeed, ".ps1")

    try {
      Move-Item -Path $tempScriptSeed -Destination $tempScript -Force
      Move-Item -Path $tempEnvSeed -Destination $tempEnv -Force
      Move-Item -Path $tempExportSeed -Destination $tempExportScript -Force

      $exportScriptContent = @(
        'param([string]$OutputPath)'
        '[System.Environment]::GetEnvironmentVariables().GetEnumerator() |'
        '  Sort-Object Key |'
        '  ForEach-Object { ''{0}={1}'' -f $_.Key, $_.Value } |'
        '  Set-Content -Path $OutputPath -Encoding UTF8'
      ) -join [Environment]::NewLine

      Set-Content -Path $tempExportScript -Value $exportScriptContent -Encoding ASCII

      $scriptContent = @(
        '@echo off'
        "call `"$vsDevCmd`" -arch=x64 >nul"
        'if errorlevel 1 exit /b %errorlevel%'
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -File `"$tempExportScript`" `"$tempEnv`""
        'if errorlevel 1 exit /b %errorlevel%'
      ) -join [Environment]::NewLine

      Set-Content -Path $tempScript -Value $scriptContent -Encoding ASCII
      & $tempScript
      if ($LASTEXITCODE -ne 0) {
        throw "Failed to initialize the Visual Studio developer environment."
      }

      $environmentDump = Get-Content -Path $tempEnv
    }
    finally {
      Remove-Item $tempScript, $tempEnv, $tempExportScript -ErrorAction SilentlyContinue
    }

    if ($LASTEXITCODE -ne 0) {
      throw "Failed to initialize the Visual Studio developer environment."
    }

    foreach ($line in $environmentDump) {
      $separatorIndex = $line.IndexOf('=')
      if ($separatorIndex -lt 1) {
        continue
      }

      $name = $line.Substring(0, $separatorIndex)
      $value = $line.Substring($separatorIndex + 1)
      Set-Item -Path "Env:$name" -Value $value
    }
  }
}

function Get-VsToolPath {
  param([string]$ToolName)

  $toolBinDir = Get-VsToolBinDir
  if (-not $toolBinDir) {
    throw "Could not determine the active Visual Studio C/C++ tool directory. The developer environment is missing both VCToolsInstallDir and a usable VCINSTALLDIR."
  }

  $toolPath = Join-Path $toolBinDir $ToolName
  if (-not (Test-Path $toolPath)) {
    throw "Expected Visual Studio tool was not found: $toolPath"
  }

  return $toolPath
}

function Get-VsToolBinDir {
  $candidateDirs = @()

  if ($env:VCToolsInstallDir) {
    $candidateDirs += (Join-Path $env:VCToolsInstallDir "bin\Hostx64\x64")
  }

  if ($env:VCINSTALLDIR) {
    $msvcRoot = Join-Path $env:VCINSTALLDIR "Tools\MSVC"
    if (Test-Path $msvcRoot) {
      $candidateDirs += Get-ChildItem -Path $msvcRoot -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName "bin\Hostx64\x64" }
    }
  }

  foreach ($candidateDir in $candidateDirs | Select-Object -Unique) {
    if (Test-Path $candidateDir) {
      return [System.IO.Path]::GetFullPath($candidateDir)
    }
  }

  return $null
}

function Get-PreferredCCompilerPath {
  $preferredTools = @("clang-cl.exe", "cl.exe")
  $toolBinDir = Get-VsToolBinDir

  if ($toolBinDir) {
    foreach ($toolName in $preferredTools) {
      $toolPath = Join-Path $toolBinDir $toolName
      if (Test-Path $toolPath) {
        return [System.IO.Path]::GetFullPath($toolPath)
      }
    }
  }

  foreach ($toolName in $preferredTools) {
    $command = Get-Command $toolName -CommandType Application -ErrorAction SilentlyContinue
    if ($command) {
      return [System.IO.Path]::GetFullPath($command.Source)
    }
  }

  if ($toolBinDir) {
    throw "Could not find clang-cl.exe or cl.exe in the active Visual Studio toolchain directory: $toolBinDir"
  }

  throw "Could not find clang-cl.exe or cl.exe. The Visual Studio developer environment did not expose a usable tool directory, and neither compiler was available on PATH."
}

function Get-PreferredCMakePath {
  $candidatePaths = @()
  $installationPath = $null

  try {
    $installationPath = Get-VsInstallationPath
  }
  catch {
    $installationPath = $null
  }

  if ($installationPath) {
    $candidatePaths += (Join-Path $installationPath "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe")
  }

  $commands = Get-Command cmake -CommandType Application -All -ErrorAction SilentlyContinue
  foreach ($command in $commands) {
    if ($command.Source) {
      $candidatePaths += $command.Source
    }
  }

  foreach ($candidatePath in $candidatePaths | Select-Object -Unique) {
    if (-not $candidatePath) {
      continue
    }

    if ($candidatePath -match '\\Strawberry\\c\\bin\\') {
      continue
    }

    if (Test-Path $candidatePath) {
      return [System.IO.Path]::GetFullPath($candidatePath)
    }
  }

  throw "Could not find a usable cmake.exe. Install Visual Studio CMake tools or a standalone CMake, and ensure Strawberry Perl's cmake is not shadowing it."
}

function Get-PreferredNinjaPath {
  $candidatePaths = @()
  $installationPath = $null

  try {
    $installationPath = Get-VsInstallationPath
  }
  catch {
    $installationPath = $null
  }

  if ($installationPath) {
    $candidatePaths += (Join-Path $installationPath "Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe")
  }

  $commands = Get-Command ninja -CommandType Application -All -ErrorAction SilentlyContinue
  foreach ($command in $commands) {
    if ($command.Source) {
      $candidatePaths += $command.Source
    }
  }

  foreach ($candidatePath in $candidatePaths | Select-Object -Unique) {
    if (-not $candidatePath) {
      continue
    }

    if ($candidatePath -match '\\Strawberry\\c\\bin\\') {
      continue
    }

    if (Test-Path $candidatePath) {
      return [System.IO.Path]::GetFullPath($candidatePath)
    }
  }

  throw "Could not find a usable ninja.exe. Install Ninja or the Visual Studio CMake tools, and ensure Strawberry Perl's toolchain is not shadowing it."
}

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

function Find-GitWindowsPerl {
  # Locate the perl.exe that ships with Git for Windows without any extra install.
  $gitCmd = Get-Command git -ErrorAction SilentlyContinue
  if ($gitCmd) {
    # git.exe lives in <git-root>\bin\ or <git-root>\cmd\; perl is at <git-root>\usr\bin\perl.exe
    $gitRoot = Split-Path (Split-Path $gitCmd.Source -Parent) -Parent
    $candidate = Join-Path $gitRoot "usr\bin\perl.exe"
    if (Test-Path $candidate) {
      return $candidate
    }
  }

  # Fall back to well-known default Git for Windows install prefixes
  $candidates = @(
    "$env:ProgramFiles\Git\usr\bin\perl.exe",
    "${env:ProgramFiles(x86)}\Git\usr\bin\perl.exe",
    "$env:LocalAppData\Programs\Git\usr\bin\perl.exe"
  )
  foreach ($candidate in $candidates) {
    if ($candidate -and (Test-Path $candidate)) {
      return $candidate
    }
  }

  return $null
}

function Ensure-PerlInterpreter {
  # Accept any perl already on PATH (Strawberry, Git, MSYS2, ActivePerl, …)
  if (Get-Command perl -ErrorAction SilentlyContinue) {
    Write-Host "perl already available"
    return
  }

  # Git for Windows ships a full-featured Perl with all required core modules.
  # Since Git is needed for the vcpkg checkout it is available on virtually
  # every developer machine without a separate install.
  $gitPerl = Find-GitWindowsPerl
  if ($gitPerl) {
    Write-Host "Using Perl bundled with Git for Windows: $gitPerl"
    $env:PATH = "$(Split-Path $gitPerl -Parent);$env:PATH"
    return
  }

  # Last resort: install Strawberry Perl via winget.
  Write-Host "No Perl interpreter found on PATH or in Git for Windows. Installing StrawberryPerl.StrawberryPerl via winget."
  winget install --id StrawberryPerl.StrawberryPerl -e --accept-package-agreements --accept-source-agreements --silent
  if ($LASTEXITCODE -ne 0) {
    throw "winget failed to install StrawberryPerl.StrawberryPerl (exit $LASTEXITCODE)."
  }
}

function Ensure-WindowsHostTools {
  Ensure-PerlInterpreter
  Install-WingetPackage -CommandName pkg-config -PackageId bloodrock.pkg-config-lite
  Install-WingetPackage -CommandName ninja -PackageId Ninja-build.Ninja
  Install-WingetPackageForCommands -CommandNames @("win_flex", "win_bison") -PackageId WinFlexBison.win_flex_bison
}

function Ensure-DependencySources {
  param(
    [string]$ResolvedVcpkgRoot
  )

  if (-not (Test-Path $ResolvedVcpkgRoot)) {
    git clone https://github.com/microsoft/vcpkg.git $ResolvedVcpkgRoot
  }
}

function Invoke-Vcpkg {
  param(
    [string]$ResolvedVcpkgRoot,
    [string[]]$Arguments,
    [string]$BuildDirectory
  )

  $vcpkgExe = Join-Path $ResolvedVcpkgRoot "vcpkg.exe"
  & $vcpkgExe @Arguments
  if ($LASTEXITCODE -ne 0) {
    Show-VcpkgFailureHints -ResolvedVcpkgRoot $ResolvedVcpkgRoot -BuildDirectory $BuildDirectory
    exit $LASTEXITCODE
  }
}

function Invoke-WindowsBootstrap {
  param(
    [string]$ResolvedVcpkgRoot,
    [string]$TripletName,
    [string]$ResolvedBinaryCacheDir,
    [switch]$ForceLibspectrumRebuild
  )

  Ensure-WindowsHostTools
  Get-VsDevCmdPath | Out-Null
  Ensure-DependencySources -ResolvedVcpkgRoot $ResolvedVcpkgRoot
  Assert-NoRepoVcpkgLockOwner -ResolvedVcpkgRoot $ResolvedVcpkgRoot
  Show-VcpkgHints -ResolvedVcpkgRoot $ResolvedVcpkgRoot -ResolvedBinaryCacheDir $ResolvedBinaryCacheDir

  $vcpkgExe = Join-Path $ResolvedVcpkgRoot "vcpkg.exe"
  if (-not (Test-Path $vcpkgExe)) {
    & (Join-Path $ResolvedVcpkgRoot "bootstrap-vcpkg.bat")
    if ($LASTEXITCODE -ne 0) {
      exit $LASTEXITCODE
    }
  }

  if ($ForceLibspectrumRebuild) {
    Write-Host "Internal libspectrum rebuild requested. Existing generated libspectrum outputs will be regenerated during the CMake build."
  }

  Write-Host ""
  Write-Host "Installed host tools and bootstrapped vcpkg for $TripletName."
  Write-Host "Dependency installation will run once during the CMake configure step via the repository manifest."
}

function Invoke-CMakeConfigure {
  param(
    [string]$BuildDirectory,
    [string]$ResolvedVcpkgRoot,
    [string]$TripletName,
    [switch]$PortablePackage,
    [switch]$StandaloneLibretro,
    [switch]$ForceLibspectrumRebuild
  )

  if ($ForceLibspectrumRebuild) {
    $libspectrumBuildDir = Join-Path $BuildDirectory "external\libspectrum"
    if (Test-Path $libspectrumBuildDir) {
      Remove-Item -Recurse -Force $libspectrumBuildDir
    }
  }

  Assert-NoRepoVcpkgLockOwner -ResolvedVcpkgRoot $ResolvedVcpkgRoot

  $cmakePath = Get-PreferredCMakePath
  $ninjaPath = Get-PreferredNinjaPath
  $compilerPath = Get-PreferredCCompilerPath
  $cachePath = Join-Path $BuildDirectory "CMakeCache.txt"
  $toolchainPath = Join-Path $ResolvedVcpkgRoot 'scripts/buildsystems/vcpkg.cmake'

  Write-Host "Running CMake configure in $BuildDirectory..."
  Write-Host "Manifest dependency installation and configure output will follow."

  $cmakeArgs = @(
    "-S", $RepoRoot,
    "-B", $BuildDirectory,
    "-G", "Ninja",
    "-DCMAKE_MAKE_PROGRAM=$ninjaPath",
    "-DCMAKE_C_COMPILER=$compilerPath",
    "-DVCPKG_TARGET_TRIPLET=$TripletName",
    "-DVCPKG_INSTALL_OPTIONS=",
    "-DFUSE_USE_INTERNAL_LIBSPECTRUM=OFF"
  )

  if (-not (Test-Path $cachePath)) {
    $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchainPath"
  }

  if ($PortablePackage) {
    $cmakeArgs += @(
      "-DCMAKE_BUILD_TYPE=Release",
      "-DCPACK_GENERATOR=ZIP",
      "-DFUSE_PORTABLE_PACKAGE=ON"
    )
  }
  elseif ($StandaloneLibretro) {
    $cmakeArgs += "-DCMAKE_BUILD_TYPE=Release"
  }

  & $cmakePath @cmakeArgs
  if ($LASTEXITCODE -ne 0) {
    Show-VcpkgFailureHints -ResolvedVcpkgRoot $ResolvedVcpkgRoot -BuildDirectory $BuildDirectory
    exit $LASTEXITCODE
  }
}

function Invoke-CMakeBuild {
  param(
    [string]$BuildDirectory,
    [string]$Target
  )

  $cmakePath = Get-PreferredCMakePath

  if ($Target) {
    Write-Host "Running CMake build target '$Target'..."
  }
  else {
    Write-Host "Building Fuse..."
  }

  $buildArgs = @("--build", $BuildDirectory)
  if ($Target) {
    $buildArgs += @("--target", $Target)
  }

  & $cmakePath @buildArgs
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}

function Invoke-GeneratedSettingsSanityCheck {
  param([string]$BuildDirectory)

  Write-Host "Checking generated settings metadata..."

  $settingsPath = Join-Path $BuildDirectory "settings.c"
  if (-not (Test-Path $settingsPath)) {
    Write-Error "Expected generated settings source not found: $settingsPath"
    exit 1
  }

  $settingsText = [System.IO.File]::ReadAllText($settingsPath)

  foreach ($expectedKey in @('startupshader', 'startupshaderparameters')) {
    $expectedLiteral = '"' + $expectedKey + '"'
    if (-not $settingsText.Contains($expectedLiteral)) {
      Write-Error "Generated settings metadata is missing the expected config key $expectedLiteral in $settingsPath"
      exit 1
    }
  }

  if ($settingsText -match '"startupshader(?:parameters)?\r?\n"') {
    Write-Error "Generated settings metadata contains a CRLF-corrupted shader config key in $settingsPath"
    exit 1
  }

  Write-Host "Generated settings metadata check passed"
}

function Invoke-SmokeTest {
  param([string]$BuildDirectory)

  Write-Host "Running runtime smoke test..."

  $exePath = Join-Path $BuildDirectory "fuse.exe"
  if (-not (Test-Path $exePath)) {
    Write-Error "Expected build output not found: $exePath"
    exit 1
  }

  $smokeStdout = Join-Path $BuildDirectory "smoke-output.txt"
  $smokeStderr = Join-Path $BuildDirectory "smoke-error.txt"
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
}

function Invoke-LibretroPackage {
  param(
    [string]$BuildDirectory,
    [string]$TripletName
  )

  $dllPath = Join-Path $BuildDirectory 'fuse-sdl3_libretro.dll'
  $infoPath = Join-Path $BuildDirectory 'fuse-sdl3_libretro.info'
  $packageOutputDir = Join-Path $RepoRoot 'build-win'
  $projectVersion = Get-RepoProjectVersion
  $architectureLabel = Get-WindowsPackageArchitectureLabel -TripletName $TripletName
  $archiveName = "fuse-sdl3_libretro-$projectVersion-Windows-$architectureLabel.zip"
  $archivePath = Join-Path $packageOutputDir $archiveName
  $legacyArchivePaths = @(
    (Join-Path $packageOutputDir 'fuse-sdl3_libretro-windows-x64.zip'),
    (Join-Path $BuildDirectory 'fuse-sdl3_libretro-windows-x64.zip'),
    (Join-Path $BuildDirectory $archiveName)
  )
  $stagingDir = Join-Path $BuildDirectory 'libretro-package'

  if (-not (Test-Path $dllPath)) {
    Write-Error "Expected libretro core not found: $dllPath"
    exit 1
  }

  if (-not (Test-Path $infoPath)) {
    Write-Error "Expected libretro info file not found: $infoPath"
    exit 1
  }

  New-Item -ItemType Directory -Force -Path $packageOutputDir | Out-Null
  Remove-Item -Recurse -Force $stagingDir -ErrorAction SilentlyContinue
  Remove-Item $archivePath -Force -ErrorAction SilentlyContinue
  foreach ($legacyArchivePath in $legacyArchivePaths | Select-Object -Unique) {
    if ($legacyArchivePath -ne $archivePath) {
      Remove-Item $legacyArchivePath -Force -ErrorAction SilentlyContinue
    }
  }
  New-Item -ItemType Directory -Force -Path $stagingDir | Out-Null

  Copy-Item $dllPath $stagingDir
  Copy-Item $infoPath $stagingDir

  Compress-Archive -Path (Join-Path $stagingDir '*') -DestinationPath $archivePath -CompressionLevel Optimal -Force
  Remove-Item -Recurse -Force $stagingDir -ErrorAction SilentlyContinue

  Write-Host "Libretro package generated: $archivePath"
}

function Read-TextFileShared {
  param(
    [string]$Path,
    [int]$RetryCount = 10,
    [int]$DelayMilliseconds = 200
  )

  for ($attempt = 0; $attempt -lt $RetryCount; $attempt++) {
    try {
      $stream = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite
      )

      try {
        $reader = New-Object System.IO.StreamReader($stream)
        try {
          return $reader.ReadToEnd()
        }
        finally {
          $reader.Dispose()
        }
      }
      finally {
        $stream.Dispose()
      }
    }
    catch {
      if ($attempt -eq ($RetryCount - 1)) {
        throw
      }

      Start-Sleep -Milliseconds $DelayMilliseconds
    }
  }

  return ""
}

function Invoke-ShaderSmokeTest {
  param(
    [string]$BuildDirectory,
    [string]$PresetPath,
    [int]$TimeoutSeconds = 5
  )

  if (-not $PresetPath) {
    $PresetPath = Get-ChildItem -Path (Join-Path $RepoRoot 'shaders') -Filter *.glslp -File -ErrorAction SilentlyContinue |
      Sort-Object Name |
      Select-Object -First 1 -ExpandProperty FullName
  }

  Write-Host "Running shader smoke test with $(Split-Path $PresetPath -Leaf)..."

  $exePath = Join-Path $BuildDirectory "fuse.exe"
  if (-not (Test-Path $exePath)) {
    Write-Error "Expected build output not found: $exePath"
    exit 1
  }

  if (-not (Test-Path $PresetPath)) {
    Write-Error "Expected shader preset not found: $PresetPath"
    exit 1
  }

  $shaderStdout = Join-Path $BuildDirectory "shader-smoke-output.txt"
  $shaderStderr = Join-Path $BuildDirectory "shader-smoke-error.txt"
  Remove-Item $shaderStdout, $shaderStderr -ErrorAction SilentlyContinue

  $process = Start-Process -FilePath $exePath `
    -ArgumentList @('--clear-startup-shader', '--startup-shader', $PresetPath) `
    -WorkingDirectory $BuildDirectory `
    -RedirectStandardOutput $shaderStdout `
    -RedirectStandardError $shaderStderr `
    -PassThru

  Start-Sleep -Seconds $TimeoutSeconds
  $process.Refresh()

  $stderrText = ""
  if (Test-Path $shaderStderr) {
    $stderrText = Read-TextFileShared -Path $shaderStderr
  }

  $shaderFailurePattern = 'startup shader preset .*ignored|falling back to the standard SDL renderer path|OpenGL setup failed'

  if ($stderrText -match $shaderFailurePattern) {
    if (-not $process.HasExited) {
      Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
      $process.WaitForExit()
    }

    if (Test-Path $shaderStdout) { Get-Content $shaderStdout }
    if (Test-Path $shaderStderr) { Get-Content $shaderStderr }
    Write-Error "Shader smoke test reported shader initialization fallback or failure"
    exit 1
  }

  if ($process.HasExited) {
    if (Test-Path $shaderStdout) { Get-Content $shaderStdout }
    if (Test-Path $shaderStderr) { Get-Content $shaderStderr }
    Write-Error "Shader smoke test exited early with code $($process.ExitCode)"
    exit $process.ExitCode
  }

  Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
  $process.WaitForExit()

  Write-Host "Shader smoke test passed: $(Split-Path $PresetPath -Leaf) initialized without shader fallback"
}

if ($BootstrapOnly -and $ConfigureOnly) {
  throw "-BootstrapOnly and -ConfigureOnly are mutually exclusive."
}

$resolvedVcpkgRoot = Resolve-RepoPath $VcpkgRoot
$buildDirPath = Resolve-RepoPath $BuildDir
$resolvedBinaryCacheDir = $null
if ($BinaryCacheDir) {
  $resolvedBinaryCacheDir = Resolve-RepoPath $BinaryCacheDir
}

if ($Uninstall) {
  $manifest = Join-Path $buildDirPath "install_manifest.txt"
  if (-not (Test-Path $manifest)) {
    Write-Error "No install manifest found: $manifest`nRun with -Install first."
    exit 1
  }
  Get-Content $manifest | ForEach-Object {
    Remove-Item $_ -Force -Verbose -ErrorAction SilentlyContinue
  }
  exit 0
}

Set-RepoVcpkgEnvironment -ResolvedVcpkgRoot $resolvedVcpkgRoot -ResolvedBinaryCacheDir $resolvedBinaryCacheDir

if (-not $SkipBootstrap) {
  Invoke-WindowsBootstrap `
    -ResolvedVcpkgRoot $resolvedVcpkgRoot `
    -ResolvedBinaryCacheDir $resolvedBinaryCacheDir `
    -TripletName $Triplet `
    -ForceLibspectrumRebuild:$RebuildLibspectrum
}

if ($BootstrapOnly) {
  exit 0
}

Import-VsDevEnvironment
Set-RepoVcpkgEnvironment -ResolvedVcpkgRoot $resolvedVcpkgRoot -ResolvedBinaryCacheDir $resolvedBinaryCacheDir
$portablePackage = $Package -and -not $Libretro
Invoke-CMakeConfigure -BuildDirectory $buildDirPath -ResolvedVcpkgRoot $resolvedVcpkgRoot -TripletName $Triplet -PortablePackage:$portablePackage -StandaloneLibretro:$Libretro -ForceLibspectrumRebuild:$RebuildLibspectrum

if ($ConfigureOnly) {
  exit 0
}

if ($Libretro) {
  Invoke-CMakeBuild -BuildDirectory $buildDirPath -Target 'fuse-libretro'
}
else {
  Invoke-CMakeBuild -BuildDirectory $buildDirPath
}
Invoke-GeneratedSettingsSanityCheck -BuildDirectory $buildDirPath

if ($RuntimeSmokeTest) {
  Invoke-SmokeTest -BuildDirectory $buildDirPath
}


if ($ShaderSmokeTest) {
  Invoke-ShaderSmokeTest -BuildDirectory $buildDirPath
}

if ($Package) {
  if ($Libretro) {
    Invoke-LibretroPackage -BuildDirectory $buildDirPath -TripletName $Triplet
  }
  else {
    Invoke-CMakeBuild -BuildDirectory $buildDirPath -Target "package"
    Write-Host "Package archives generated in $buildDirPath"
  }
}

if ($Install) {
  $cmakePath = Get-PreferredCMakePath
  $installArgs = @("--install", $buildDirPath)
  if ($InstallPrefix) {
    $installArgs += @("--prefix", $InstallPrefix)
  }
  & $cmakePath @installArgs
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}

exit 0