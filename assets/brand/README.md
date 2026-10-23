# Minicast logo directions

**04 / Double crescent** is the selected Minicast logo. The other three monochrome vector studies remain as design history.

| Direction | Geometry | Character |
| --- | --- | --- |
| **01 / Split** | Two circular segments separated by a diagonal gap. | Balanced and recognizable. |
| **02 / Crescent** | A disk with one offset circular subtraction. | Soft and quiet. |
| **03 / Half** | One diagonal semicircle, centered by its bounds. | The simplest silhouette. |
| **04 / Double crescent** | Two copies of Crescent, with the second rotated 180 degrees. | Paired, balanced and directional. |

All SVGs have a transparent background, a 64 × 64 view box, and a single `currentColor` path. Set CSS `color` to use the same mark in light or dark contexts; the default is charcoal. The original circle radius is 26 units; the split is 5 units wide. Double crescent moves the first crescent by (-9, +9), rotates its partner 180 degrees around (32, 32), then scales the pair by 52/67 about the center. Its ink bounds are 6–58, with a 4.39-unit minimum gap; the shapes do not overlap.

`directions.png` compares all three on light and dark backgrounds, with 16, 24, and 32 px samples beneath each large mark. `render-directions.ps1` generates this sheet offline using Windows System.Drawing with 3× supersampling; it does not open any windows. The renderer uses the same circle geometry as the SVG paths.

`double-crescent.png` compares the original Crescent and the flipped pair in the same light/dark and small-size treatments. Generate it with `./render-directions.ps1 -DoubleCrescent`. The compound path uses winding fill in the renderer to match SVG's default fill rule.

`scripts/generate-brand-icons.ps1` reads the selected SVG directly and produces `minicast.ico`, `tray-light.ico`, and `tray-dark.ico`. Each contains 16/20/24/32/40/48/64/128/256 px RGBA PNG frames. The app icon uses a charcoal rounded tile for contrast across Explorer surfaces; the tray marks stay transparent and switch with the Windows taskbar theme. The approved crescent geometry is retained.

`icon-frames.png` displays frames decoded from the completed ICOs at native size. `icon-validation.json` records dimensions and alpha-pixel counts. Generation and verification are offline and do not open windows. The build embeds the three icon groups and verifies their payloads directly from PE bytes; runtime does not need the SVG, ICO files, previews or generator.
