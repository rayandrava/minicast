param([switch]$DoubleCrescent)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

# Offline render only: no windows, browser, or desktop interaction.
$scale = 3
$width = if ($DoubleCrescent) { 988 } else { 1440 }
$height = 940
$bitmap = [Drawing.Bitmap]::new($width * $scale, $height * $scale)
$graphics = [Drawing.Graphics]::FromImage($bitmap)
$graphics.ScaleTransform($scale, $scale)
$graphics.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.TextRenderingHint = [Drawing.Text.TextRenderingHint]::AntiAliasGridFit
$graphics.Clear([Drawing.ColorTranslator]::FromHtml('#e6e8eb'))

function Brush([string]$hex) { [Drawing.SolidBrush]::new([Drawing.ColorTranslator]::FromHtml($hex)) }
function Text([string]$value, [float]$x, [float]$y, [float]$size, [string]$color, [bool]$bold = $false) {
    $style = if ($bold) { [Drawing.FontStyle]::Bold } else { [Drawing.FontStyle]::Regular }
    $font = [Drawing.Font]::new('Segoe UI', $size, $style, [Drawing.GraphicsUnit]::Pixel)
    $brush = Brush $color
    $graphics.DrawString($value, $font, $brush, $x, $y)
    $brush.Dispose()
    $font.Dispose()
}

function LogoPath([int]$direction) {
    $path = [Drawing.Drawing2D.GraphicsPath]::new()
    if ($direction -eq 0) {
        $halfChord = [Math]::Sqrt(26 * 26 - 2.5 * 2.5) / [Math]::Sqrt(2)
        $offset = 2.5 / [Math]::Sqrt(2)
        $start = [Math]::Atan2($offset - $halfChord, $offset + $halfChord) * 180 / [Math]::PI
        $sweep = 90 - 2 * $start
        $path.AddArc(6, 6, 52, 52, $start, $sweep)
        $path.CloseFigure()
        $path.StartFigure()
        $path.AddArc(6, 6, 52, 52, $start + 180, $sweep)
        $path.CloseFigure()
    } elseif ($direction -eq 1) {
        $halfChord = [Math]::Sqrt(26 * 26 - 11 * 11 / 2) / [Math]::Sqrt(2)
        $firstX = 39 - $halfChord
        $firstY = 25 - $halfChord
        $lastX = 39 + $halfChord
        $lastY = 25 + $halfChord
        $outerStart = [Math]::Atan2($firstY - 30.5, $firstX - 33.5) * 180 / [Math]::PI
        $outerEnd = [Math]::Atan2($lastY - 30.5, $lastX - 33.5) * 180 / [Math]::PI
        $innerStart = [Math]::Atan2($lastY - 19.5, $lastX - 44.5) * 180 / [Math]::PI
        $innerEnd = [Math]::Atan2($firstY - 19.5, $firstX - 44.5) * 180 / [Math]::PI + 360
        $path.AddArc(7.5, 4.5, 52, 52, $outerStart, $outerEnd - $outerStart - 360)
        $path.AddArc(18.5, -6.5, 52, 52, $innerStart, $innerEnd - $innerStart)
        $path.CloseFigure()
    } elseif ($direction -eq 2) {
        $center = 32 - (26 - 26 / [Math]::Sqrt(2)) / 2
        $path.AddArc($center - 26, $center - 26, 52, 52, -45, 180)
        $path.CloseFigure()
    } else {
        # Two copies of 02, with one rotated 180 degrees. Separate their tips
        # before fitting the pair into the same 6..58 ink bounds as the disk.
        $crescent = LogoPath 1
        $offset = [Drawing.Drawing2D.Matrix]::new()
        $offset.Translate(-9, 9)
        $crescent.Transform($offset)
        $path.AddPath($crescent, $false)
        $flip = [Drawing.Drawing2D.Matrix]::new()
        $flip.RotateAt(180, [Drawing.PointF]::new(32, 32))
        $crescent.Transform($flip)
        $path.AddPath($crescent, $false)
        $fit = [Drawing.Drawing2D.Matrix]::new()
        $fit.Translate(32, 32)
        $fit.Scale(52 / 67, 52 / 67)
        $fit.Translate(-32, -32)
        $path.Transform($fit)
        $path.FillMode = [Drawing.Drawing2D.FillMode]::Winding
        $fit.Dispose()
        $flip.Dispose()
        $offset.Dispose()
        $crescent.Dispose()
    }
    return ,$path
}

function Mark([int]$direction, [float]$x, [float]$y, [float]$size, [string]$color) {
    $path = LogoPath $direction
    $matrix = [Drawing.Drawing2D.Matrix]::new()
    $matrix.Translate($x, $y)
    $matrix.Scale($size / 64, $size / 64)
    $path.Transform($matrix)
    $brush = Brush $color
    $graphics.FillPath($brush, $path)
    $brush.Dispose()
    $matrix.Dispose()
    $path.Dispose()
}

function SmallSizes([int]$direction, [float]$x, [float]$y, [string]$markColor, [string]$labelColor) {
    $sizes = @(16, 24, 32)
    for ($index = 0; $index -lt $sizes.Count; $index++) {
        $size = $sizes[$index]
        $center = $x + 90 * $index
        Mark $direction ($center - $size / 2) ($y + (32 - $size) / 2) $size $markColor
        Text "$size" ($center - 8) ($y + 46) 14 $labelColor
    }
}

Text 'MINICAST' 54 36 14 '#6b7078' $true
if ($DoubleCrescent) {
    Text 'A crescent, flipped.' 51 61 36 '#17191c' $true
    Text 'Original and double-crescent variation.' 54 116 17 '#6b7078'
} else {
    Text 'Circle studies' 51 61 36 '#17191c' $true
    Text 'Three simple directions. One color.' 54 116 17 '#6b7078'
}

$names = @('01 / Split', '02 / Crescent', '03 / Half', '04 / Double crescent')
$descriptions = @('A disk, divided.', 'A circular cutout.', 'One clean half.', 'Two crescents. Opposite directions.')
$directions = if ($DoubleCrescent) { @(1, 3) } else { @(0, 1, 2) }
for ($column = 0; $column -lt $directions.Count; $column++) {
    $direction = $directions[$column]
    $x = 54 + $column * 452
    $cardWidth = 428
    $lightBrush = Brush '#f7f8fa'
    $darkBrush = Brush '#24272c'
    $graphics.FillRectangle($lightBrush, $x, 170, $cardWidth, 342)
    $graphics.FillRectangle($darkBrush, $x, 512, $cardWidth, 342)
    $lightBrush.Dispose()
    $darkBrush.Dispose()
    Text $names[$direction] ($x + 24) 193 21 '#17191c' $true
    Text $descriptions[$direction] ($x + 24) 223 15 '#747a83'
    Mark $direction ($x + ($cardWidth - 128) / 2) 277 128 '#17191c'
    SmallSizes $direction ($x + $cardWidth / 2 - 90) 430 '#17191c' '#747a83'
    Text 'DARK' ($x + 24) 535 12 '#9a9fa8' $true
    Mark $direction ($x + ($cardWidth - 128) / 2) 600 128 '#f2f4f7'
    SmallSizes $direction ($x + $cardWidth / 2 - 90) 758 '#f2f4f7' '#9a9fa8'
}

Text 'Transparent SVG originals  /  16, 24 and 32 px samples' 54 885 15 '#6b7078'
if (!$DoubleCrescent) { Text 'Direction studies - no final selection' 1103 887 13 '#6b7078' }

$output = [Drawing.Bitmap]::new($width, $height)
$outputGraphics = [Drawing.Graphics]::FromImage($output)
$outputGraphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$outputGraphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$outputGraphics.DrawImage($bitmap, 0, 0, $width, $height)
$filename = if ($DoubleCrescent) { 'double-crescent.png' } else { 'directions.png' }
$output.Save((Join-Path $PSScriptRoot $filename), [Drawing.Imaging.ImageFormat]::Png)
$outputGraphics.Dispose()
$output.Dispose()
$graphics.Dispose()
$bitmap.Dispose()
Write-Output "Rendered assets/brand/$filename without opening any windows."
