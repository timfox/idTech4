<#
Build Bullet (third_party/bullet3) as **static** libraries with **/MT** (Release) or **/MTd** (Debug)
to match Doom 3’s `MultiThreaded` / `MultiThreadedDebug` runtime.

Requires: Visual Studio 2022 C++ x64, CMake.

  pwsh -File third_party\build-bullet-msvc.ps1
  pwsh -File third_party\build-bullet-msvc.ps1 -Configuration Debug

After success, import `neo\_Bullet.props` in your vcxproj (DoomDLL already does) and ensure submodules:

  git submodule update --init --recursive
#>
param(
    [ValidateSet('Release', 'Debug')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$src = Join-Path $repo 'third_party\bullet3'
$out = Join-Path $repo 'third_party\bullet'
$build = Join-Path $out ('msbuild_' + $Configuration)
$libDest = Join-Path $out ('lib\' + $Configuration)

if (-not (Test-Path (Join-Path $src 'CMakeLists.txt'))) {
    Write-Error "Missing bullet3 sources at $src — init submodule: git submodule update --init --recursive"
}

New-Item -ItemType Directory -Force -Path $libDest | Out-Null

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { Write-Error "vswhere.exe not found" }
$install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $install) { Write-Error "MSVC x64 toolset not found" }
$vcvars = Join-Path $install 'VC\Auxiliary\Build\vcvars64.bat'
if (-not (Test-Path $vcvars)) { Write-Error "vcvars64.bat missing" }

$mt = if ($Configuration -eq 'Debug') { 'MultiThreadedDebug' } else { 'MultiThreaded' }
$cfg = if ($Configuration -eq 'Debug') { 'Debug' } else { 'Release' }

$cmakeCommon = @(
    '-S', $src,
    '-B', $build,
    '-G', 'Visual Studio 17 2022',
    '-A', 'x64',
    '-DBUILD_SHARED_LIBS=OFF',
    '-DUSE_MSVC_RUNTIME_LIBRARY_DLL=OFF',
    "-DCMAKE_MSVC_RUNTIME_LIBRARY=$mt",
    '-DBUILD_CPU_DEMOS=OFF',
    '-DBUILD_BULLET3=OFF',
    '-DBUILD_OPENGL3_DEMOS=OFF',
    '-DBUILD_BULLET2_DEMOS=OFF',
    '-DBUILD_EXTRAS=OFF',
    '-DBUILD_UNIT_TESTS=OFF',
    '-DINSTALL_LIBS=OFF',
    '-DBUILD_ENET=OFF',
    '-DBUILD_CLSOCKET=OFF',
    '-DBUILD_PYBULLET=OFF'
)

$batch = @"
call "$vcvars" >nul
cmake $($cmakeCommon -join ' ')
if errorlevel 1 exit /b 1
cmake --build "$build" --config $cfg --parallel
if errorlevel 1 exit /b 1
"@
cmd /c $batch
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

function Find-Lib($name) {
    $hit = Get-ChildItem -Path $build -Recurse -Filter $name -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\lib\\' -or $_.FullName -match "\\$cfg\\" } |
        Select-Object -First 1
    if (-not $hit) {
        $hit = Get-ChildItem -Path $build -Recurse -Filter $name -ErrorAction SilentlyContinue | Select-Object -First 1
    }
    if (-not $hit) { Write-Error "Built $name not found under $build" }
    return $hit.FullName
}

foreach ($lib in @('LinearMath.lib', 'BulletCollision.lib', 'BulletDynamics.lib')) {
    $p = Find-Lib $lib
    Copy-Item -Force $p (Join-Path $libDest $lib)
    Write-Host "-> $(Join-Path $libDest $lib)"
}

Write-Host "Bullet static libs ready: $libDest"
