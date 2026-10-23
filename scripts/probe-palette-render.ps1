# Optional local GPU diagnostic; not a desktop/visual test or launch benchmark.
# Run build.ps1 first to generate version, Lucide and brand resources.
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
$compiler = Join-Path $projectRoot '.tools\zig-x86_64-windows-0.14.1\zig.exe'
if (!(Test-Path -LiteralPath 'build\version.h') -or !(Test-Path -LiteralPath 'build\minicast.res')) {
    throw 'Run build.ps1 before the hidden rendering diagnostic.'
}
$env:ZIG_GLOBAL_CACHE_DIR = Join-Path $projectRoot '.cache\zig'
$env:ZIG_LOCAL_CACHE_DIR = Join-Path $projectRoot '.cache\local'
$sources = @('scripts/probe-palette-render.cpp','src/core.cpp','src/calculator.cpp','src/platform.cpp','src/camera.cpp','src/lucide.cpp','src/surface.cpp','src/app_icons.cpp','src/brand_icons.cpp','src/theme.cpp','src/local_currency.cpp','src/startup.cpp','src/search_input.cpp','build/minicast.res')
$libraries = @('-luser32','-lgdi32','-ldwmapi','-ld2d1','-ldwrite','-ld3d11','-ldxgi','-ldcomp','-lshell32','-lole32','-loleaut32','-luuid','-lcomctl32','-lcrypt32','-lwinhttp','-lmfplat','-lmfreadwrite','-lmfuuid','-lmf','-lwindowscodecs','-lpropsys','-lwtsapi32','-lpsapi','-ladvapi32','-luxtheme','-limm32')
& $compiler 'c++' '-std=c++20' '-O2' '-DNDEBUG' '-DUNICODE' '-D_UNICODE' '-DWINVER=0x0A00' '-Isrc' '-Ibuild' '-target' 'x86_64-windows-gnu' @sources @libraries '-o' 'build/probe-palette-render.exe'
if ($LASTEXITCODE -ne 0) { throw 'Rendering diagnostic compilation failed.' }
& '.\build\probe-palette-render.exe'
if ($LASTEXITCODE -ne 0) { throw 'Hidden rendering diagnostic failed.' }
