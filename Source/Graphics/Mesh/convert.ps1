$path = "Mesh.cpp"
if (Test-Path $path) {
    $content = Get-Content $path -Raw -Encoding UTF8
    [System.IO.File]::WriteAllText((Resolve-Path $path), $content, [System.Text.Encoding]::GetEncoding(932))
    Write-Output "Conversion Done"
} else {
    Write-Error "File not found in current directory"
}
