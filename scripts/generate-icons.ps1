$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'assets\lucide'
$lines = [System.Collections.Generic.List[string]]::new()
$lines.Add('// Generated from vendored Lucide 0.468.0 SVGs. See assets/lucide/LICENSE.')
$lines.Add('static const IconData iconData[] = {')
foreach ($file in (Get-ChildItem -LiteralPath $source -Filter '*.svg' | Sort-Object Name)) {
    [xml]$svg = Get-Content -LiteralPath $file.FullName -Raw
    $lines.Add('    {"' + $file.BaseName + '", {')
    foreach ($shape in $svg.DocumentElement.ChildNodes) {
        if ($shape.NodeType -ne [System.Xml.XmlNodeType]::Element) { continue }
        $path = ''
        switch ($shape.LocalName) {
            'path' { $path = $shape.GetAttribute('d') }
            'line' { $path = 'M' + $shape.GetAttribute('x1') + ' ' + $shape.GetAttribute('y1') + ' L' + $shape.GetAttribute('x2') + ' ' + $shape.GetAttribute('y2') }
            'polyline' { $path = 'M' + $shape.GetAttribute('points') }
            'rect' {
                $x = if ($shape.HasAttribute('x')) { $shape.GetAttribute('x') } else { '0' }
                $y = if ($shape.HasAttribute('y')) { $shape.GetAttribute('y') } else { '0' }
                $rx = if ($shape.HasAttribute('rx')) { $shape.GetAttribute('rx') } else { '0' }
                $lines.Add('        {Shape::Rect, "", {' + $x + ',' + $y + ',' + $shape.GetAttribute('width') + ',' + $shape.GetAttribute('height') + ',' + $rx + '}},')
                continue
            }
            'circle' {
                $lines.Add('        {Shape::Circle, "", {' + $shape.GetAttribute('cx') + ',' + $shape.GetAttribute('cy') + ',' + $shape.GetAttribute('r') + '}},')
                continue
            }
            default { throw ('Unsupported Lucide element: ' + $shape.LocalName) }
        }
        if ($path) { $lines.Add('        {Shape::Path, "' + $path + '", {}},') }
    }
    $lines.Add('    }},')
}
$lines.Add('};')
[System.IO.File]::WriteAllLines((Join-Path $root 'src\lucide_data.inc'),$lines,[System.Text.UTF8Encoding]::new($false))
