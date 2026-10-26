param([switch]$Test,[string]$Output='dist/Minicast.exe',[string]$Repository='')
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
Set-Location -LiteralPath $projectRoot
$version = (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
if ($version -notmatch '^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)(?:-([0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*))?$') {
    throw 'VERSION must contain major.minor.patch with an optional prerelease suffix.'
}
$versionParts = @([int]$Matches[1], [int]$Matches[2], [int]$Matches[3])
if (@($versionParts | Where-Object { $_ -gt 65535 }).Count) { throw 'PE version components must not exceed 65535.' }
if ($version.Contains('-')) {
    foreach ($identifier in $version.Substring($version.IndexOf('-') + 1).Split('.')) {
        if ($identifier -match '^0[0-9]+$') { throw 'Numeric prerelease identifiers must not have leading zeroes.' }
    }
}
if ($Repository -and $Repository -notmatch '^[A-Za-z0-9][A-Za-z0-9-]{0,38}/[A-Za-z0-9][A-Za-z0-9_.-]{0,99}$') {
    throw 'Repository must be a GitHub owner/name, without a URL or path.'
}
$releasesUrl = if ($Repository) { "https://github.com/$Repository/releases" } else { '' }
$compiler = Join-Path $projectRoot '.tools\zig-x86_64-windows-0.14.1\zig.exe'
if (!(Test-Path -LiteralPath $compiler)) { throw 'Run scripts\bootstrap.ps1 to install the local compiler.' }
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $projectRoot '.cache\zig'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $projectRoot '.cache\local'
New-Item -ItemType Directory -Force -Path 'build','dist' | Out-Null
$Output = [IO.Path]::GetFullPath($Output)
$outputDirectory = Split-Path -Parent $Output
New-Item -ItemType Directory -Force -Path $outputDirectory | Out-Null
$versionTuple = ($versionParts + 0) -join ','
$versionFlags = if ($version.Contains('-')) { '0x00000002L' } else { '0x00000000L' }
$versionHeader = @"
// Generated from VERSION by build.ps1. Do not edit or commit.
#pragma once
#define MINICAST_VERSION_STRING "$version"
#define MINICAST_VERSION_WSTRING L"$version"
#define MINICAST_VERSION_RESOURCE_STRING "$version\0"
#define MINICAST_VERSION_TUPLE $versionTuple
#define MINICAST_VERSION_FILEFLAGS $versionFlags
#define MINICAST_RELEASES_URL_WSTRING L"$releasesUrl"
"@
[IO.File]::WriteAllText((Join-Path $projectRoot 'build\version.h'), $versionHeader, [Text.Encoding]::ASCII)
& (Join-Path $projectRoot 'scripts\generate-icons.ps1')
& (Join-Path $projectRoot 'scripts\generate-brand-icons.ps1')
& $compiler 'rc' '/I' 'src' '/I' 'build' '/I' 'assets/brand' '/fo' 'build/minicast.res' 'src/minicast.rc'
if ($LASTEXITCODE -ne 0) { throw 'Application icon resource compilation failed' }
$common = @('c++','-std=c++20','-O2','-DNDEBUG','-DUNICODE','-D_UNICODE','-DWINVER=0x0A00','-Wall','-Wextra','-Wno-missing-field-initializers','-Isrc','-Ibuild','-target','x86_64-windows-gnu')
if ($Test) {
    & $compiler @common 'tests/core_tests.cpp' 'src/core.cpp' 'src/calculator.cpp' '-o' 'build/core_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed' }
    & '.\build\core_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    & $compiler @common 'tests/ranking_tests.cpp' '-o' 'build/ranking_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Ranking test compilation failed' }
    & '.\build\ranking_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Ranking tests failed' }
    & $compiler @common 'tests/shortcut_tests.cpp' '-o' 'build/shortcut_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Shortcut test compilation failed' }
    & '.\build\shortcut_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Shortcut tests failed' }
    & $compiler @common 'tests/history_tests.cpp' '-o' 'build/history_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Clipboard history test compilation failed' }
    & '.\build\history_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Clipboard history tests failed' }
    & $compiler @common 'tests/text_layout_cache_tests.cpp' '-ldwrite' '-luuid' '-o' 'build/text_layout_cache_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Text cache test compilation failed' }
    & '.\build\text_layout_cache_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Text cache tests failed' }
    & $compiler @common 'tests/smooth_scroll_tests.cpp' '-o' 'build/smooth_scroll_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Smooth scroll test compilation failed' }
    & '.\build\smooth_scroll_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Smooth scroll tests failed' }
    & $compiler @common 'tests/search_metrics_tests.cpp' '-o' 'build/search_metrics_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Search DPI test compilation failed' }
    & '.\build\search_metrics_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Search DPI tests failed' }
    & $compiler @common 'tests/app_shell_tests.cpp' '-o' 'build/app_shell_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Application target test compilation failed' }
    & '.\build\app_shell_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Application target tests failed' }
    & $compiler @common 'tests/instance_identity_tests.cpp' '-o' 'build/instance_identity_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Instance identity test compilation failed' }
    & '.\build\instance_identity_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Instance identity tests failed' }
    & $compiler @common 'tests/storage_tests.cpp' 'src/platform.cpp' 'src/core.cpp' '-lwindowscodecs' '-o' 'build/storage_tests.exe' '-lshell32' '-lole32' '-luuid' '-luser32' '-lcrypt32' '-lwinhttp'
    if ($LASTEXITCODE -ne 0) { throw 'Storage test compilation failed' }
    & '.\build\storage_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Storage tests failed' }
    & $compiler @common 'tests/clipboard_image_tests.cpp' 'src/platform.cpp' 'src/core.cpp' '-o' 'build/clipboard_image_tests.exe' '-lshell32' '-lole32' '-luuid' '-luser32' '-lcrypt32' '-lwinhttp' '-lwindowscodecs'
    if ($LASTEXITCODE -ne 0) { throw 'Clipboard image test compilation failed' }
    & '.\build\clipboard_image_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Clipboard image tests failed' }
    & $compiler @common 'tests/camera_image_tests.cpp' 'src/camera.cpp' 'src/platform.cpp' 'src/core.cpp' '-o' 'build/camera_image_tests.exe' '-lole32' '-luuid' '-lwindowscodecs' '-lmfplat' '-lmfreadwrite' '-lmfuuid' '-lmf' '-luser32' '-lshell32' '-lcrypt32' '-lwinhttp'
    if ($LASTEXITCODE -ne 0) { throw 'Camera image test compilation failed' }
    & '.\build\camera_image_tests.exe'
    if ($LASTEXITCODE -ne 0) { throw 'Camera image tests failed' }
}
& $compiler @common 'src/main.cpp' 'src/core.cpp' 'src/calculator.cpp' 'src/platform.cpp' 'src/camera.cpp' 'src/lucide.cpp' 'src/surface.cpp' 'src/app_icons.cpp' 'src/brand_icons.cpp' 'src/theme.cpp' 'src/local_currency.cpp' 'src/startup.cpp' 'src/search_input.cpp' 'build/minicast.res' '-o' $Output '-Wl,--subsystem,windows' '-municode' '-s' '-luser32' '-lgdi32' '-ldwmapi' '-ld2d1' '-ldwrite' '-ld3d11' '-ldxgi' '-ldcomp' '-lshell32' '-lole32' '-loleaut32' '-luuid' '-lcomctl32' '-lcrypt32' '-lwinhttp' '-lmfplat' '-lmfreadwrite' '-lmfuuid' '-lmf' '-lwindowscodecs' '-lpropsys' '-lwtsapi32' '-lpsapi' '-ladvapi32' '-luxtheme' '-limm32'
if ($LASTEXITCODE -ne 0) { throw 'Application compilation failed' }
# Verify the artifact instead of trusting compiler flags: this Zig driver can
# ignore -mwindows. A console-subsystem build opens an unwanted terminal.
$previewBytes = [System.IO.File]::ReadAllBytes((Get-Item -LiteralPath $Output).FullName)
$peOffset = [BitConverter]::ToInt32($previewBytes,60)
if ([BitConverter]::ToUInt16($previewBytes,$peOffset+24+68) -ne 2) { throw 'Minicast must use the Windows GUI subsystem' }
if ([BitConverter]::ToUInt16($previewBytes,$peOffset+4) -ne 0x8664) { throw 'Minicast must be built for Windows x64.' }
$versionInfo = [Diagnostics.FileVersionInfo]::GetVersionInfo($Output)
if ($versionInfo.ProductVersion -ne $version -or $versionInfo.FileVersion -ne $version) {
    throw 'Executable version metadata does not match VERSION.'
}
& (Join-Path $projectRoot 'scripts\verify-brand-resources.ps1') -Executable $Output
Copy-Item -LiteralPath 'assets/lucide/LICENSE' -Destination (Join-Path $outputDirectory 'LUCIDE-LICENSE.txt')
Get-Item -LiteralPath $Output | Select-Object FullName,Length
