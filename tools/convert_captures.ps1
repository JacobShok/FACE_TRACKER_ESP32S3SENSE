# Converts every .bmp in ../captures to .png, for uploading to Edge Impulse.
# Usage: powershell -File tools/convert_captures.ps1

Add-Type -AssemblyName System.Drawing

$capturesDir = Join-Path $PSScriptRoot "..\captures"
$bmpFiles = Get-ChildItem -Path $capturesDir -Filter "*.bmp"

if ($bmpFiles.Count -eq 0) {
    Write-Output "No .bmp files found in $capturesDir"
    exit
}

$converted = 0
foreach ($file in $bmpFiles) {
    $pngPath = [System.IO.Path]::ChangeExtension($file.FullName, "png")
    try {
        $bmp = [System.Drawing.Image]::FromFile($file.FullName)
        $bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
        $bmp.Dispose()
        $converted++
    } catch {
        Write-Output "FAILED: $($file.Name) - $($_.Exception.Message)"
    }
}

Write-Output "Converted $converted / $($bmpFiles.Count) files to PNG in $capturesDir"
