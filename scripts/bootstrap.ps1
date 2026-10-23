param([switch]$VerifyOnly)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsRoot = Join-Path $projectRoot '.tools'
$distribution = 'zig-x86_64-windows-0.14.1'
$compilerRoot = Join-Path $toolsRoot $distribution
$archive = Join-Path $toolsRoot "$distribution.zip"
# Official checksum: https://ziglang.org/download/index.json (0.14.1, x86_64-windows).
$expected = '554f5378228923ffd558eac35e21af020c73789d87afeabf4bfd16f2e6feed2c'
if (!(Test-Path -LiteralPath $archive)) {
    $legacyArchive = Join-Path $toolsRoot 'zig.zip'
    if (Test-Path -LiteralPath $legacyArchive) { $archive = $legacyArchive }
}
if (!(Test-Path -LiteralPath $archive)) {
    if ($VerifyOnly) { throw 'The pinned Zig archive is missing. Run scripts\bootstrap.ps1 first.' }
    New-Item -ItemType Directory -Force -Path $toolsRoot | Out-Null
    Invoke-WebRequest "https://ziglang.org/download/0.14.1/$distribution.zip" -OutFile $archive
}
# Never skip validation for an existing download or a restored cache.
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expected) {
    throw "Compiler archive checksum mismatch: $archive. Obtain a fresh official archive before retrying."
}
if (!(Test-Path -LiteralPath $compilerRoot)) {
    if ($VerifyOnly) { throw 'The compiler is not extracted. Run scripts\bootstrap.ps1 first.' }
    Expand-Archive -LiteralPath $archive -DestinationPath $toolsRoot
}

# A valid ZIP does not make an independently modified extracted cache trustworthy.
# Verify all compiler/library files against the pinned archive before executing Zig.
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
$knownFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$hasher = [Security.Cryptography.SHA256]::Create()
try {
    foreach ($entry in $zip.Entries) {
        if (!$entry.Name) { continue }
        $file = [IO.Path]::GetFullPath((Join-Path $toolsRoot $entry.FullName))
        if (!$file.StartsWith($compilerRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Unexpected path in the compiler archive.'
        }
        [void]$knownFiles.Add($file)
        if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Compiler cache is incomplete: $file" }
        if ((Get-Item -LiteralPath $file).Length -ne $entry.Length) { throw "Compiler cache differs from the verified archive: $file" }
        $entryStream = $entry.Open()
        try { $entryHash = [BitConverter]::ToString($hasher.ComputeHash($entryStream)).Replace('-', '') }
        finally { $entryStream.Dispose() }
        if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entryHash) {
            throw "Compiler cache differs from the verified archive: $file"
        }
    }
    foreach ($file in Get-ChildItem -LiteralPath $compilerRoot -File -Recurse -Force) {
        if (!$knownFiles.Contains($file.FullName)) { throw "Unexpected file in compiler cache: $($file.FullName)" }
    }
} finally {
    $hasher.Dispose()
    $zip.Dispose()
}
Write-Output 'Verified Zig 0.14.1 for Windows x64 (archive and extracted files).'
