# make_ico.ps1
# Converts a PNG (even one renamed to .ico) into a proper multi-resolution ICO file.
# Stores the original 512x512 pixels untouched; smaller sizes use high-quality bicubic
# downsampling with gamma correction disabled to preserve the original colors.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\make_ico.ps1
#        (run from the project root, or adjust $src / $dst below)

Add-Type -AssemblyName System.Drawing

$src = Join-Path $PSScriptRoot '..\src\assets\app_icon.ico'
$dst = $src

$srcImage = [System.Drawing.Image]::FromFile((Resolve-Path $src))
Write-Host "Source: $($srcImage.Width) x $($srcImage.Height) px"

# Sizes to embed. The 512 entry stores the original bytes with zero resampling.
# ICO directory entries use a single byte for width/height; 0 encodes 256 (and by
# convention also any larger PNG-in-ICO entry on Windows Vista+).
$sizes = @(16, 32, 48, 256, 512)

$pngBlobs = @()
foreach ($sz in $sizes) {
    if ($sz -ge $srcImage.Width) {
        # Source is same size or smaller — save original bytes directly, no resampling
        $ms = New-Object System.IO.MemoryStream
        $srcImage.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $pngBlobs += ,($sz, $ms.ToArray())
        $ms.Dispose()
        Write-Host "  ${sz}x${sz}: stored original (no resampling)"
    } else {
        # Downsample with highest-quality settings and gamma correction disabled
        $bmp = New-Object System.Drawing.Bitmap($sz, $sz, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g   = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CompositingMode    = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
        $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $g.InterpolationMode  = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.SmoothingMode      = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
        $g.PixelOffsetMode    = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        # ImageAttributes: disable gamma correction so colors are copied as-is
        $ia = New-Object System.Drawing.Imaging.ImageAttributes
        $ia.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
        $ia.SetGamma(1.0, [System.Drawing.Imaging.ColorAdjustType]::Bitmap)

        $dst_rect = New-Object System.Drawing.Rectangle(0, 0, $sz, $sz)
        $g.DrawImage($srcImage, $dst_rect, 0, 0, $srcImage.Width, $srcImage.Height,
                     [System.Drawing.GraphicsUnit]::Pixel, $ia)
        $ia.Dispose()
        $g.Dispose()

        $ms = New-Object System.IO.MemoryStream
        $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
        $pngBlobs += ,($sz, $ms.ToArray())
        $ms.Dispose()
        $bmp.Dispose()
        Write-Host "  ${sz}x${sz}: downsampled with bicubic + gamma=1.0"
    }
}
$srcImage.Dispose()

# Write ICO binary: ICONDIR + N×ICONDIRENTRY + PNG blobs
# PNG-in-ICO is supported on Windows Vista and later.
$tmp = $dst + '.tmp'
$fs  = New-Object System.IO.FileStream($tmp, [System.IO.FileMode]::Create)
$bw  = New-Object System.IO.BinaryWriter($fs)

$n          = $pngBlobs.Count
$headerSize = 6 + $n * 16
$offset     = $headerSize

# ICONDIR header
$bw.Write([uint16]0)    # reserved
$bw.Write([uint16]1)    # type = 1 (icon)
$bw.Write([uint16]$n)

# ICONDIRENTRY per size
foreach ($blob in $pngBlobs) {
    $sz   = $blob[0]
    $data = $blob[1]
    $dim  = if ($sz -ge 256) { 0 } else { $sz }   # 0 = 256+ in ICO spec
    $bw.Write([byte]$dim)
    $bw.Write([byte]$dim)
    $bw.Write([byte]0)              # color count (0 = true color)
    $bw.Write([byte]0)              # reserved
    $bw.Write([uint16]1)            # color planes
    $bw.Write([uint16]32)           # bits per pixel
    $bw.Write([uint32]$data.Length)
    $bw.Write([uint32]$offset)
    $offset += $data.Length
}

# Image data
foreach ($blob in $pngBlobs) {
    $bw.Write($blob[1])
}

$bw.Close()
$fs.Close()

Move-Item -Force $tmp (Resolve-Path $src)
Write-Host "Done -> $src ($n sizes embedded)"