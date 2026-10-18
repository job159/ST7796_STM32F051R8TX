# Generates 2bpp anti-aliased proportional fonts as C arrays for the ILI9488 GUI.
# Renders glyphs with GDI+ (grayscale AA), trims ink bbox, packs 2 bits/pixel.
param(
  [string]$OutC = "C:\Users\a2105\STM32CubeIDE\workspace_1.14.1\TFT_ILI9488_STM32F051\Core\Src\fonts.c",
  [string]$OutH = "C:\Users\a2105\STM32CubeIDE\workspace_1.14.1\TFT_ILI9488_STM32F051\Core\Inc\fonts.h"
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function New-FontSafe([string]$family, [single]$px, [System.Drawing.FontStyle]$style) {
  try {
    $f = New-Object System.Drawing.Font($family, $px, $style, [System.Drawing.GraphicsUnit]::Pixel)
    if ($f.Name -ne $family) { throw "fallback" }
    return $f
  } catch {
    return New-Object System.Drawing.Font("Segoe UI", $px, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
  }
}

function Render-FontSet {
  param([string]$Name, [System.Drawing.Font]$Font, [string]$Charset, [bool]$IsList)

  $pad = 16
  $cellW = [int]([math]::Ceiling($Font.Size * 3)) + 2*$pad
  $cellH = [int]([math]::Ceiling($Font.GetHeight() * 2)) + 8
  $bmp = New-Object System.Drawing.Bitmap($cellW, $cellH)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::AntiAliasGridFit
  $fmt = [System.Drawing.StringFormat]::new([System.Drawing.StringFormat]::GenericTypographic)
  $fmt.FormatFlags = $fmt.FormatFlags -bor [System.Drawing.StringFormatFlags]::MeasureTrailingSpaces
  $white = [System.Drawing.Brushes]::White

  $lineH = [int][math]::Ceiling($Font.GetHeight($g))

  $glyphs = @()   # objects: {ch, adv, xoff, yoff, w, h, rows(byte[])}
  $dataBytes = New-Object System.Collections.Generic.List[byte]

  foreach ($ch in $Charset.ToCharArray()) {
    $s = [string]$ch
    $szF = $g.MeasureString($s, $Font, [System.Drawing.PointF]::new(0,0), $fmt)
    $adv = [int][math]::Round($szF.Width)
    if ($adv -lt 1) { $adv = [int][math]::Round($Font.Size * 0.28) }

    $g.Clear([System.Drawing.Color]::Black)
    $g.DrawString($s, $Font, $white, [System.Drawing.PointF]::new($pad, 0), $fmt)

    # scan ink bbox (quantized > 0)
    $minX = $cellW; $minY = $cellH; $maxX = -1; $maxY = -1
    for ($y = 0; $y -lt $cellH; $y++) {
      for ($x = 0; $x -lt $cellW; $x++) {
        $v = $bmp.GetPixel($x, $y).R
        $q = [int][math]::Round($v / 85.0)
        if ($q -gt 0) {
          if ($x -lt $minX) { $minX = $x }
          if ($x -gt $maxX) { $maxX = $x }
          if ($y -lt $minY) { $minY = $y }
          if ($y -gt $maxY) { $maxY = $y }
        }
      }
    }

    if ($maxX -lt 0) {  # blank glyph (space)
      $glyphs += [pscustomobject]@{ ch=$ch; off=$dataBytes.Count; adv=$adv; xoff=0; yoff=0; w=0; h=0 }
      continue
    }

    $w = $maxX - $minX + 1
    $h = $maxY - $minY + 1
    $rowBytes = [int][math]::Ceiling($w / 4.0)
    $off = $dataBytes.Count
    for ($y = $minY; $y -le $maxY; $y++) {
      $rb = New-Object byte[] $rowBytes
      for ($x = $minX; $x -le $maxX; $x++) {
        $v = $bmp.GetPixel($x, $y).R
        $q = [int][math]::Round($v / 85.0)
        $ix = $x - $minX
        $shift = 6 - 2 * ($ix % 4)
        $rb[[int][math]::Floor($ix / 4)] = $rb[[int][math]::Floor($ix / 4)] -bor ($q -shl $shift)
      }
      $dataBytes.AddRange($rb)
    }
    $glyphs += [pscustomobject]@{ ch=$ch; off=$off; adv=$adv; xoff=($minX-$pad); yoff=$minY; w=$w; h=$h }
  }

  $g.Dispose(); $bmp.Dispose()
  return [pscustomobject]@{ Name=$Name; LineH=$lineH; Glyphs=$glyphs; Data=$dataBytes; Charset=$Charset; IsList=$IsList }
}

function Emit-CArrayBytes([System.Text.StringBuilder]$sb, [string]$name, $bytes) {
  [void]$sb.AppendLine("static const uint8_t $name[] = {")
  $line = "  "
  for ($i = 0; $i -lt $bytes.Count; $i++) {
    $line += ("0x{0:X2}," -f $bytes[$i])
    if ((($i+1) % 24) -eq 0) { [void]$sb.AppendLine($line); $line = "  " }
  }
  if ($line.Trim().Length -gt 0) { [void]$sb.AppendLine($line) }
  [void]$sb.AppendLine("};")
}

# ---------------- fonts spec ----------------
# Non-ASCII glyphs map to fixed single-byte codes on the C side:
#   U+00B0 degree -> 0xB0      U+03A9 ohm -> 0xEA
$CharByteMap = @{ 0xB0 = 0xB0; 0x3A9 = 0xEA }

$ASCII  = -join (32..126 | ForEach-Object { [char]$_ })
$EXTSET = $ASCII + [char]0xB0 + [char]0x3A9
$BIGSET = "0123456789.:%+-" + [char]0xB0 + "C "   # degree sign 0xB0

$fSm  = New-FontSafe "Segoe UI Semibold" 13 ([System.Drawing.FontStyle]::Regular)
$fMd  = New-FontSafe "Segoe UI Semibold" 17 ([System.Drawing.FontStyle]::Regular)
$fBig = New-FontSafe "Segoe UI Semibold" 34 ([System.Drawing.FontStyle]::Regular)

Write-Host "Rendering SM ($($fSm.Name))..."
$sm  = Render-FontSet -Name "sm"  -Font $fSm  -Charset $EXTSET -IsList $true
Write-Host "Rendering MD ($($fMd.Name))..."
$md  = Render-FontSet -Name "md"  -Font $fMd  -Charset $EXTSET -IsList $true
Write-Host "Rendering BIG ($($fBig.Name))..."
$big = Render-FontSet -Name "big" -Font $fBig -Charset $BIGSET -IsList $true

# ---------------- emit C ----------------
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("/* Auto-generated anti-aliased 2bpp proportional fonts (Segoe UI Semibold).")
[void]$sb.AppendLine(" * Generated by gen_fonts.ps1 - do not edit by hand. */")
[void]$sb.AppendLine('#include "fonts.h"')
[void]$sb.AppendLine("")

$total = 0
foreach ($f in @($sm, $md, $big)) {
  Emit-CArrayBytes $sb ("font_" + $f.Name + "_data") $f.Data
  $total += $f.Data.Count
  [void]$sb.AppendLine("")
  [void]$sb.AppendLine("static const GfxGlyph font_$($f.Name)_glyphs[] = {")
  foreach ($gl in $f.Glyphs) {
    $c = [int][char]$gl.ch
    $lbl = if ($c -ge 32 -and $c -lt 127) { [string]$gl.ch } else { ("\x{0:X2}" -f $c) }
    [void]$sb.AppendLine(("  {{ {0,5}, {1,2}, {2,3}, {3,2}, {4,2}, {5,3} }}, /* '{6}' */" -f $gl.off, $gl.adv, $gl.xoff, $gl.w, $gl.h, $gl.yoff, $lbl))
  }
  [void]$sb.AppendLine("};")
  $glTotal = $f.Glyphs.Count * 8
  $total += $glTotal
  [void]$sb.AppendLine("")
}

# charset list escaping; non-ASCII chars become separate string literals
# so a following hex digit can't extend the \x escape
function Esc-List([string]$set) {
  ($set.ToCharArray() | ForEach-Object {
    $c = [int]$_
    if ($c -ge 32 -and $c -lt 127) {
      if ($_ -eq '"' -or $_ -eq '\') { "\$_" } else { [string]$_ }
    } else {
      $b = $CharByteMap[$c]
      if ($null -eq $b) { throw "no byte mapping for U+$('{0:X4}' -f $c)" }
      ('" "\x{0:X2}" "' -f $b)
    }
  }) -join ''
}
$smList  = Esc-List $EXTSET
$mdList  = Esc-List $EXTSET
$bigList = Esc-List $BIGSET

[void]$sb.AppendLine("const GfxFont FONT_SM  = { font_sm_data,  font_sm_glyphs,  ""$smList"", 0, 0, $($sm.LineH) };")
[void]$sb.AppendLine("const GfxFont FONT_MD  = { font_md_data,  font_md_glyphs,  ""$mdList"", 0, 0, $($md.LineH) };")
[void]$sb.AppendLine("const GfxFont FONT_BIG = { font_big_data, font_big_glyphs, ""$bigList"", 0, 0, $($big.LineH) };")

$hdr = @"
/* Auto-generated anti-aliased 2bpp proportional fonts. See gen_fonts.ps1. */
#ifndef FONTS_H
#define FONTS_H
#include <stdint.h>

typedef struct {
  uint16_t off;    /* offset into bitmap data           */
  uint8_t  adv;    /* horizontal advance in pixels      */
  int8_t   xoff;   /* ink left offset from pen position */
  uint8_t  w, h;   /* ink bitmap size                   */
  int8_t   yoff;   /* ink top offset from line top      */
} GfxGlyph;

typedef struct {
  const uint8_t  *data;    /* 2bpp rows, ceil(w/4) bytes per row  */
  const GfxGlyph *glyphs;
  const char     *list;    /* NULL: range font [first..last]      */
  uint8_t first, last;
  uint8_t lineH;
} GfxFont;

extern const GfxFont FONT_SM;   /* 13 px labels / status      */
extern const GfxFont FONT_MD;   /* 17 px buttons / titles     */
extern const GfxFont FONT_BIG;  /* 34 px numeric readouts     */

#endif
"@

Set-Content -Path $OutC -Value $sb.ToString() -Encoding ascii
Set-Content -Path $OutH -Value $hdr -Encoding ascii
Write-Host ("OK  fonts.c bytes(data+tables) ~= {0}  lineH: sm={1} md={2} big={3}" -f $total, $sm.LineH, $md.LineH, $big.LineH)
Write-Host ("Glyph counts: sm={0} md={1} big={2}" -f $sm.Glyphs.Count, $md.Glyphs.Count, $big.Glyphs.Count)
