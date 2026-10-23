param([Parameter(Mandatory=$true)][string]$Executable)
$ErrorActionPreference = 'Stop'

# Inspect PE data only. Never load the executable or invoke an entry point.
$peBytes = [IO.File]::ReadAllBytes((Get-Item -LiteralPath $Executable).FullName)
function U16([int]$offset) { [BitConverter]::ToUInt16($peBytes, $offset) }
function U32([int]$offset) { [BitConverter]::ToUInt32($peBytes, $offset) }
$peHeader = U32 60
if ((U32 $peHeader) -ne 0x4550) { throw 'Invalid PE signature' }
$optional = $peHeader + 24
if ((U16 $optional) -ne 0x20b) { throw 'Expected PE32+ executable' }
$sectionCount = U16 ($peHeader + 6)
$sectionTable = $optional + (U16 ($peHeader + 20))

function FileOffset([uint32]$rva) {
    for ($index = 0; $index -lt $sectionCount; $index++) {
        $section = $sectionTable + 40 * $index
        $start = U32 ($section + 12)
        $length = [Math]::Max((U32 ($section + 8)), (U32 ($section + 16)))
        if ($rva -ge $start -and $rva -lt [long]$start + $length) {
            return [int]((U32 ($section + 20)) + $rva - $start)
        }
    }
    throw "RVA $rva is outside the PE sections"
}
$resourceRva = U32 ($optional + 112 + 16)
if (!$resourceRva) { throw 'Executable has no icon resources' }
$resourceRoot = FileOffset $resourceRva

function ResourceEntry([int]$directory, [int]$id) {
    $count = (U16 ($directory + 12)) + (U16 ($directory + 14))
    for ($index = 0; $index -lt $count; $index++) {
        $entry = $directory + 16 + 8 * $index
        if ((U32 $entry) -eq $id) { return (U32 ($entry + 4)) }
    }
    throw "Missing resource ID $id"
}
function ResourceBytes([int]$type, [int]$id) {
    $directory = $resourceRoot
    foreach ($key in @($type, $id)) {
        $entry = ResourceEntry $directory $key
        if (!($entry -band 2147483648)) { throw 'Expected resource directory' }
        $directory = $resourceRoot + ($entry -band 2147483647)
    }
    if ((U16 ($directory + 12)) + (U16 ($directory + 14)) -ne 1) { throw 'Expected one icon resource language' }
    $entry = U32 ($directory + 20)
    if ($entry -band 2147483648) { throw 'Expected resource data entry' }
    $data = $resourceRoot + $entry
    $offset = FileOffset (U32 $data)
    $length = U32 ($data + 4)
    if (!$length -or [long]$offset + $length -gt $peBytes.Length) { throw 'Invalid icon resource size' }
    $bytes = [byte[]]::new($length)
    [Array]::Copy($peBytes, $offset, $bytes, 0, $length)
    return ,$bytes
}

$iconFiles = [ordered]@{101='minicast.ico'; 102='tray-light.ico'; 103='tray-dark.ico'}
$imageCount = 0
$sha = [Security.Cryptography.SHA256]::Create()
try {
    foreach ($item in $iconFiles.GetEnumerator()) {
        $ico = [IO.File]::ReadAllBytes((Join-Path $PSScriptRoot "../assets/brand/$($item.Value)"))
        $group = ResourceBytes 14 $item.Key
        for ($index = 0; $index -lt 6; $index++) {
            if ($group[$index] -ne $ico[$index]) { throw 'Icon group header mismatch' }
        }
        $count = [BitConverter]::ToUInt16($ico, 4)
        if ($group.Length -ne 6 + 14 * $count) { throw 'Icon group entry count mismatch' }
        for ($index = 0; $index -lt $count; $index++) {
            $icoEntry = 6 + 16 * $index
            $groupEntry = 6 + 14 * $index
            for ($field = 0; $field -lt 12; $field++) {
                if ($group[$groupEntry + $field] -ne $ico[$icoEntry + $field]) { throw 'Icon dimensions or metadata mismatch' }
            }
            $bytes = ResourceBytes 3 ([BitConverter]::ToUInt16($group, $groupEntry + 12))
            $length = [BitConverter]::ToUInt32($ico, $icoEntry + 8)
            $offset = [BitConverter]::ToUInt32($ico, $icoEntry + 12)
            if ($bytes.Length -ne $length) { throw 'Embedded icon image size mismatch' }
            $expected = [Convert]::ToBase64String($sha.ComputeHash($ico, $offset, $length))
            $actual = [Convert]::ToBase64String($sha.ComputeHash($bytes))
            if ($expected -ne $actual) { throw 'Embedded icon image differs from its ICO asset' }
            $imageCount++
        }
    }
} finally { $sha.Dispose() }
Write-Output "Verified $($iconFiles.Count) embedded icon groups ($imageCount images) without loading the executable."
