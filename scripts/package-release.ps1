param([string]$OutputDirectory='dist/release',[string]$Repository='')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
Set-Location -LiteralPath $projectRoot
$outputRoot = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $outputRoot) {
    throw "Release output already exists: $outputRoot. Choose a new directory; existing builds are never replaced."
}
$version = (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+(?:-[0-9A-Za-z-]+(?:\.[0-9A-Za-z-]+)*)?$') {
    throw 'VERSION is not a supported release version.'
}
$copyFiles = [ordered]@{
    'LICENSE' = 'LICENSE'
    'THIRD_PARTY_NOTICES.md' = 'THIRD_PARTY_NOTICES.md'
    'README.md' = 'README.md'
    'VERSION' = 'VERSION'
    'SECURITY.md' = 'SECURITY.md'
    'CONTRIBUTING.md' = 'CONTRIBUTING.md'
    'TECHNICAL_PLAN.md' = 'TECHNICAL_PLAN.md'
    'docs/PRIVACY.md' = 'docs/PRIVACY.md'
    'docs/RELEASING.md' = 'docs/RELEASING.md'
    'docs/VALIDATION.md' = 'docs/VALIDATION.md'
    'assets/lucide/LICENSE' = 'assets/lucide/LICENSE'
    'assets/lucide/FEATHER-LICENSE' = 'assets/lucide/FEATHER-LICENSE'
    'licenses/ZIG.txt' = 'licenses/ZIG.txt'
    'licenses/LLVM-LIBCXX.txt' = 'licenses/LLVM-LIBCXX.txt'
    'licenses/LLVM-LIBCXXABI.txt' = 'licenses/LLVM-LIBCXXABI.txt'
    'licenses/LLVM-LIBUNWIND.txt' = 'licenses/LLVM-LIBUNWIND.txt'
    'licenses/MINGW-W64.txt' = 'licenses/MINGW-W64.txt'
    'licenses/MINGW-ADDITIONAL.txt' = 'licenses/MINGW-ADDITIONAL.txt'
    'licenses/MUSL.txt' = 'licenses/MUSL.txt'
}
foreach ($source in $copyFiles.Keys) {
    if (!(Test-Path -LiteralPath (Join-Path $projectRoot $source) -PathType Leaf)) {
        throw "Required release file is missing: $source"
    }
}
# Verify the downloaded archive and all extracted compiler files, without network.
& (Join-Path $PSScriptRoot 'bootstrap.ps1') -VerifyOnly
$stage = Join-Path $outputRoot 'stage'
New-Item -ItemType Directory -Path $stage | Out-Null
$executable = Join-Path $stage 'Minicast.exe'
$buildArguments = @{ Test=$true; Output=$executable }
if ($Repository) { $buildArguments.Repository = $Repository }
& (Join-Path $projectRoot 'build.ps1') @buildArguments

foreach ($source in $copyFiles.Keys) {
    $destination = Join-Path $stage $copyFiles[$source]
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $destination) | Out-Null
    Copy-Item -LiteralPath (Join-Path $projectRoot $source) -Destination $destination
}
$allowlist = @('Minicast.exe', 'LUCIDE-LICENSE.txt') + @($copyFiles.Values) | Sort-Object
$actualFiles = @(Get-ChildItem -LiteralPath $stage -File -Recurse -Force |
    ForEach-Object { $_.FullName.Substring($stage.Length + 1).Replace('\', '/') } | Sort-Object)
if (Compare-Object $allowlist $actualFiles) { throw 'Release staging contains unexpected or missing files.' }

$utf8 = [Text.UTF8Encoding]::new($false)
$fileChecksums = foreach ($name in $allowlist) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $stage $name) -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $name"
}
[IO.File]::WriteAllText((Join-Path $stage 'SHA256SUMS.txt'), ($fileChecksums -join "`n") + "`n", $utf8)
$packageName = "Minicast-$version-windows-x64.zip"
$packagePath = Join-Path $outputRoot $packageName
Add-Type -AssemblyName System.IO.Compression.FileSystem
[IO.Compression.ZipFile]::CreateFromDirectory($stage, $packagePath, [IO.Compression.CompressionLevel]::Optimal, $false)
# Inspect the archive without starting the executable.
$archive = [IO.Compression.ZipFile]::OpenRead($packagePath)
try {
    $expectedEntries = @($allowlist) + @('SHA256SUMS.txt') | Sort-Object
    $actualEntries = @($archive.Entries | Select-Object -ExpandProperty FullName | Sort-Object)
    if (Compare-Object $expectedEntries $actualEntries) { throw 'ZIP contents do not match the release allowlist.' }
} finally { $archive.Dispose() }

$sourceCommit = $null
if (Get-Command git -ErrorAction SilentlyContinue) {
    $revision = & git -C $projectRoot rev-parse --verify --quiet HEAD
    if ($LASTEXITCODE -eq 0 -and $revision -match '^[a-fA-F0-9]{40}$') { $sourceCommit = $revision.Trim() }
}
$packageHash = (Get-FileHash -LiteralPath $packagePath -Algorithm SHA256).Hash.ToLowerInvariant()
$release = [ordered]@{
    version = $version
    target = 'windows-x64'
    repository = if ($Repository) { $Repository } else { $null }
    source_commit = $sourceCommit
    package = $packageName
    sha256 = $packageHash
    authenticode_signed = $false
}
$metadataPath = Join-Path $outputRoot 'release.json'
[IO.File]::WriteAllText($metadataPath, ($release | ConvertTo-Json) + "`n", $utf8)
$metadataHash = (Get-FileHash -LiteralPath $metadataPath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $outputRoot 'SHA256SUMS.txt'),
    "$packageHash  $packageName`n$metadataHash  release.json`n", $utf8)
Write-Output "Portable release prepared: $packagePath"
Get-Item -LiteralPath $packagePath, $metadataPath, (Join-Path $outputRoot 'SHA256SUMS.txt') |
    Select-Object Name, Length
