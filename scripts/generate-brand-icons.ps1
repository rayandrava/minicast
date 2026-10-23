$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# Offline asset generation only. This script never opens a window or loads an app.
$projectRoot = Split-Path -Parent $PSScriptRoot
$brandDirectory = Join-Path $projectRoot 'assets\brand'
$sourcePath = Join-Path $brandDirectory '04-double-crescent.svg'
$sizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
[xml]$source = Get-Content -LiteralPath $sourcePath -Raw
if ($source.svg.viewBox -ne '0 0 64 64' -or @($source.svg.path).Count -ne 1 -or $source.svg.fill -ne 'currentColor') {
    throw 'Expected the approved 64 × 64 single-path double-crescent SVG.'
}

function Read-Number([string]$value) {
    return [double]::Parse($value, [Globalization.CultureInfo]::InvariantCulture)
}

function New-BrandPath {
    # The SVG is the source of truth. Read its absolute M/A/Z circular arcs instead
    # of maintaining a second hand-copied approximation of the selected logo.
    $tokens = [regex]::Matches($source.svg.path.d, '[MAZ]|[-+]?(?:\d*\.)?\d+(?:[eE][-+]?\d+)?')
    $path = [Drawing.Drawing2D.GraphicsPath]::new([Drawing.Drawing2D.FillMode]::Winding)
    $index = 0
    $currentX = 0.0
    $currentY = 0.0
    $inkLeft = $inkTop = [double]::PositiveInfinity
    $inkRight = $inkBottom = [double]::NegativeInfinity
    while ($index -lt $tokens.Count) {
        $command = $tokens[$index++].Value
        if ($command -eq 'M') {
            $currentX = Read-Number $tokens[$index++].Value
            $currentY = Read-Number $tokens[$index++].Value
            $path.StartFigure()
        } elseif ($command -eq 'A') {
            $radius = Read-Number $tokens[$index++].Value
            $radiusY = Read-Number $tokens[$index++].Value
            $rotation = Read-Number $tokens[$index++].Value
            $large = Read-Number $tokens[$index++].Value
            $sweep = Read-Number $tokens[$index++].Value
            $nextX = Read-Number $tokens[$index++].Value
            $nextY = Read-Number $tokens[$index++].Value
            if ($radius -ne $radiusY -or $rotation -ne 0) { throw 'Only circular unrotated SVG arcs are supported.' }
            $dx = ($currentX - $nextX) / 2
            $dy = ($currentY - $nextY) / 2
            $lengthSquared = $dx * $dx + $dy * $dy
            if ($lengthSquared -le 0 -or $lengthSquared -gt $radius * $radius) { throw 'Invalid circle arc.' }
            $factor = [Math]::Sqrt([Math]::Max(0.0, ($radius * $radius - $lengthSquared) / $lengthSquared))
            if ($large -eq $sweep) { $factor = -$factor }
            $centerX = ($currentX + $nextX) / 2 + $factor * $dy
            $centerY = ($currentY + $nextY) / 2 - $factor * $dx
            $start = [Math]::Atan2($currentY - $centerY, $currentX - $centerX)
            $end = [Math]::Atan2($nextY - $centerY, $nextX - $centerX)
            $angle = $end - $start
            if ($sweep -eq 0 -and $angle -gt 0) { $angle -= 2 * [Math]::PI }
            if ($sweep -eq 1 -and $angle -lt 0) { $angle += 2 * [Math]::PI }
            # Test exact circle extrema. GraphicsPath.GetBounds includes Bezier
            # control points, which extend beyond the visible arc's ink bounds.
            foreach ($candidate in @($start, $end, 0.0, ([Math]::PI / 2), [Math]::PI, (3 * [Math]::PI / 2))) {
                $travel = if ($angle -ge 0) { $candidate - $start } else { $start - $candidate }
                $travel = ($travel % (2 * [Math]::PI) + 2 * [Math]::PI) % (2 * [Math]::PI)
                if ($travel -gt [Math]::Abs($angle) + 0.00000001) { continue }
                $pointX = $centerX + $radius * [Math]::Cos($candidate)
                $pointY = $centerY + $radius * [Math]::Sin($candidate)
                $inkLeft = [Math]::Min($inkLeft, $pointX)
                $inkTop = [Math]::Min($inkTop, $pointY)
                $inkRight = [Math]::Max($inkRight, $pointX)
                $inkBottom = [Math]::Max($inkBottom, $pointY)
            }
            $path.AddArc($centerX - $radius, $centerY - $radius, 2 * $radius, 2 * $radius,
                $start * 180 / [Math]::PI, $angle * 180 / [Math]::PI)
            $currentX = $nextX
            $currentY = $nextY
        } elseif ($command -eq 'Z') {
            $path.CloseFigure()
        } else {
            throw "Unsupported SVG command: $command"
        }
    }
    if ([Math]::Abs($inkLeft - 6) -gt 0.0001 -or [Math]::Abs($inkTop - 6) -gt 0.0001 -or
        [Math]::Abs($inkRight - 58) -gt 0.0001 -or [Math]::Abs($inkBottom - 58) -gt 0.0001) {
        throw 'SVG ink bounds differ from the approved 6..58 double-crescent geometry.'
    }
    return ,$path
}

function New-RoundedTilePath {
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    $diameter = 26.0
    $path.AddArc(2, 2, $diameter, $diameter, 180, 90)
    $path.AddArc(62 - $diameter, 2, $diameter, $diameter, 270, 90)
    $path.AddArc(62 - $diameter, 62 - $diameter, $diameter, $diameter, 0, 90)
    $path.AddArc(2, 62 - $diameter, $diameter, $diameter, 90, 90)
    $path.CloseFigure()
    return ,$path
}

function New-IconPng([int]$size, [string]$color, [bool]$tile) {
    $supersample = 4
    $large = [Drawing.Bitmap]::new($size * $supersample, $size * $supersample, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [Drawing.Graphics]::FromImage($large)
    $graphics.Clear([Drawing.Color]::Transparent)
    $graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.ScaleTransform($size * $supersample / 64.0, $size * $supersample / 64.0)
    if ($tile) {
        $tilePath = New-RoundedTilePath
        $tileBrush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#24272c'))
        $graphics.FillPath($tileBrush, $tilePath)
        $tileBrush.Dispose()
        $tilePath.Dispose()
    }
    $path = New-BrandPath
    if ($tile) {
        $fit = [Drawing.Drawing2D.Matrix]::new()
        $fit.Translate(32, 32)
        $fit.Scale(0.82, 0.82)
        $fit.Translate(-32, -32)
        $path.Transform($fit)
        $fit.Dispose()
    }
    $brush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($color))
    $graphics.FillPath($brush, $path)
    $brush.Dispose()
    $path.Dispose()
    $graphics.Dispose()

    $frame = [Drawing.Bitmap]::new($size, $size, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $downsample = [Drawing.Graphics]::FromImage($frame)
    $downsample.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
    $downsample.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
    $downsample.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $downsample.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $downsample.DrawImage($large, [Drawing.Rectangle]::new(0, 0, $size, $size), 0, 0,
        $large.Width, $large.Height, [Drawing.GraphicsUnit]::Pixel)
    $downsample.Dispose()
    $large.Dispose()
    $stream = [IO.MemoryStream]::new()
    $frame.Save($stream, [Drawing.Imaging.ImageFormat]::Png)
    $bytes = $stream.ToArray()
    $stream.Dispose()
    $frame.Dispose()
    return ,$bytes
}

function Write-Icon([string]$filename, [string]$color, [bool]$tile) {
    $frames = [Collections.Generic.List[byte[]]]::new()
    foreach ($size in $sizes) { $frames.Add((New-IconPng $size $color $tile)) }
    $stream = [IO.File]::Open((Join-Path $brandDirectory $filename), [IO.FileMode]::Create, [IO.FileAccess]::Write)
    $writer = [IO.BinaryWriter]::new($stream)
    $writer.Write([uint16]0)
    $writer.Write([uint16]1)
    $writer.Write([uint16]$frames.Count)
    $offset = 6 + 16 * $frames.Count
    for ($index = 0; $index -lt $frames.Count; $index++) {
        $dimension = if ($sizes[$index] -eq 256) { 0 } else { $sizes[$index] }
        $writer.Write([byte]$dimension)
        $writer.Write([byte]$dimension)
        $writer.Write([byte]0)
        $writer.Write([byte]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]32)
        $writer.Write([uint32]$frames[$index].Length)
        $writer.Write([uint32]$offset)
        $offset += $frames[$index].Length
    }
    foreach ($bytes in $frames) { $writer.Write($bytes) }
    $writer.Dispose()
    $stream.Dispose()
}

if (-not ('MinicastBrandAlpha' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Drawing;
public static class MinicastBrandAlpha {
    public static int[] Count(Bitmap image) {
        int clear=0, partial=0, opaque=0;
        for (int y=0; y<image.Height; ++y)
            for (int x=0; x<image.Width; ++x) {
                byte alpha=image.GetPixel(x,y).A;
                if(alpha==0) ++clear;
                else if(alpha==255) ++opaque;
                else ++partial;
            }
        return new[] {clear,partial,opaque};
    }
}
'@ -ReferencedAssemblies @([Drawing.Bitmap].Assembly.Location, [Drawing.Color].Assembly.Location)
}

function Read-VerifiedIcon([string]$filename) {
    $bytes = [IO.File]::ReadAllBytes((Join-Path $brandDirectory $filename))
    if ([BitConverter]::ToUInt16($bytes, 0) -ne 0 -or [BitConverter]::ToUInt16($bytes, 2) -ne 1 -or
        [BitConverter]::ToUInt16($bytes, 4) -ne $sizes.Count) { throw "Invalid icon directory: $filename" }
    $frames = [Collections.Generic.List[object]]::new()
    $expectedOffset = 6 + 16 * $sizes.Count
    for ($index = 0; $index -lt $sizes.Count; $index++) {
        $entry = 6 + 16 * $index
        $width = if ($bytes[$entry] -eq 0) { 256 } else { [int]$bytes[$entry] }
        $height = if ($bytes[$entry + 1] -eq 0) { 256 } else { [int]$bytes[$entry + 1] }
        $length = [BitConverter]::ToUInt32($bytes, $entry + 8)
        $offset = [BitConverter]::ToUInt32($bytes, $entry + 12)
        if ($width -ne $sizes[$index] -or $height -ne $width -or $offset -ne $expectedOffset -or
            [BitConverter]::ToUInt16($bytes, $entry + 4) -ne 1 -or
            [BitConverter]::ToUInt16($bytes, $entry + 6) -ne 32 -or $offset + $length -gt $bytes.Length) {
            throw "Invalid icon frame: $filename / $index"
        }
        $png = [byte[]]::new($length)
        [Array]::Copy($bytes, $offset, $png, 0, $length)
        if ([BitConverter]::ToString($png, 0, 8) -ne '89-50-4E-47-0D-0A-1A-0A' -or
            $png[24] -ne 8 -or $png[25] -ne 6) { throw "Expected 8-bit RGBA PNG: $filename / $width" }
        $memory = [IO.MemoryStream]::new($png, $false)
        $decoded = [Drawing.Bitmap]::new($memory)
        $image = [Drawing.Bitmap]::new($decoded)
        $decoded.Dispose()
        $memory.Dispose()
        if ($image.Width -ne $width -or $image.Height -ne $height) { throw "PNG dimensions differ: $filename / $width" }
        $alpha = [MinicastBrandAlpha]::Count($image)
        if ($alpha[0] -eq 0 -or $alpha[1] -eq 0 -or $alpha[2] -eq 0 -or $image.GetPixel(0,0).A -ne 0) {
            throw "Missing transparent background or antialiased alpha: $filename / $width"
        }
        $frames.Add([pscustomobject]@{Size=$width; Image=$image; Bytes=$length; Clear=$alpha[0]; Antialiased=$alpha[1]; Opaque=$alpha[2]})
        $expectedOffset += $length
    }
    if ($expectedOffset -ne $bytes.Length) { throw "Unexpected trailing icon bytes: $filename" }
    return ,$frames
}

Write-Icon 'minicast.ico' '#f2f4f7' $true
Write-Icon 'tray-light.ico' '#17191c' $false
Write-Icon 'tray-dark.ico' '#f2f4f7' $false
$iconFiles = @('minicast.ico', 'tray-light.ico', 'tray-dark.ico')
$verified = @{}
foreach ($filename in $iconFiles) { $verified[$filename] = Read-VerifiedIcon $filename }

# Decode the completed ICOs above, then show their actual PNG frames without
# stretching. This contact sheet inspects shipped assets, not another render.
$sheet = [Drawing.Bitmap]::new(1440, 1450, [Drawing.Imaging.PixelFormat]::Format32bppArgb)
$sheetGraphics = [Drawing.Graphics]::FromImage($sheet)
$sheetGraphics.Clear([Drawing.ColorTranslator]::FromHtml('#e6e8eb'))
$sheetGraphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$sheetGraphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
$titleFont = [Drawing.Font]::new('Segoe UI', 28, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
$rowFont = [Drawing.Font]::new('Segoe UI', 17, [Drawing.FontStyle]::Bold, [Drawing.GraphicsUnit]::Pixel)
$labelFont = [Drawing.Font]::new('Segoe UI', 14, [Drawing.FontStyle]::Regular, [Drawing.GraphicsUnit]::Pixel)
$textBrush = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml('#17191c'))
$sheetGraphics.DrawString('Minicast / Selected double crescent', $titleFont, $textBrush, 40, 24)
$sheetGraphics.DrawString('Actual ICO frames, shown at native size. Transparent tray variants follow the taskbar theme.', $labelFont, $textBrush, 42, 67)
$textBrush.Dispose()
$rows = @(
    @{File='minicast.ico'; Label='Application icon / light surface'; Background='#f7f8fa'; Text='#17191c'},
    @{File='minicast.ico'; Label='Application icon / dark surface'; Background='#17191c'; Text='#f2f4f7'},
    @{File='tray-light.ico'; Label='Tray / light taskbar'; Background='#f7f8fa'; Text='#17191c'},
    @{File='tray-dark.ico'; Label='Tray / dark taskbar'; Background='#24272c'; Text='#f2f4f7'}
)
$centers = @(100, 184, 268, 352, 436, 528, 640, 830, 1130)
for ($rowIndex = 0; $rowIndex -lt $rows.Count; $rowIndex++) {
    $row = $rows[$rowIndex]
    $top = 110 + 328 * $rowIndex
    $background = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($row.Background))
    $foreground = [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($row.Text))
    $sheetGraphics.FillRectangle($background, 24, $top, 1392, 316)
    $sheetGraphics.DrawString($row.Label, $rowFont, $foreground, 48, $top + 14)
    for ($index = 0; $index -lt $sizes.Count; $index++) {
        $frame = $verified[$row.File][$index]
        $sheetGraphics.DrawImageUnscaled($frame.Image, [int]($centers[$index] - $frame.Size / 2), [int]($top + 164 - $frame.Size / 2))
        $sheetGraphics.DrawString([string]$frame.Size, $labelFont, $foreground, $centers[$index] - 10, $top + 294)
    }
    $background.Dispose()
    $foreground.Dispose()
}
$sheet.Save((Join-Path $brandDirectory 'icon-frames.png'), [Drawing.Imaging.ImageFormat]::Png)
$sheetGraphics.Dispose()
$sheet.Dispose()
$titleFont.Dispose()
$rowFont.Dispose()
$labelFont.Dispose()

$results = foreach ($filename in $iconFiles) {
    $frames = $verified[$filename]
    [pscustomobject]@{
        File=$filename
        Bytes=(Get-Item -LiteralPath (Join-Path $brandDirectory $filename)).Length
        Frames=@($frames | ForEach-Object { [pscustomobject]@{Size=$_.Size; PngBytes=$_.Bytes; ClearPixels=$_.Clear; AntialiasedPixels=$_.Antialiased; OpaquePixels=$_.Opaque} })
    }
    foreach ($frame in $frames) { $frame.Image.Dispose() }
}
$results | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $brandDirectory 'icon-validation.json') -Encoding UTF8
$results | Select-Object File,Bytes,@{Name='Frames';Expression={$_.Frames.Count}} | Format-Table -AutoSize
Write-Output 'Verified 27 RGBA PNG frames and rendered assets/brand/icon-frames.png without opening any windows.'
