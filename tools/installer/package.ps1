# Packages the installer: stub + payload zip + footer.
param(
    [Parameter(Mandatory=$true)][string]$PayloadZip,
    [Parameter(Mandatory=$true)][string]$OutFile
)
$stub = Join-Path $PSScriptRoot 'stub.exe'
if (-not (Test-Path $stub)) { throw "build stub.exe first (build.bat)" }

$s = [System.IO.File]::ReadAllBytes($stub)
$z = [System.IO.File]::ReadAllBytes($PayloadZip)
$fs = [System.IO.File]::Create($OutFile)
$fs.Write($s, 0, $s.Length)
$fs.Write($z, 0, $z.Length)
$footer = New-Object byte[] 16
[BitConverter]::GetBytes([uint64]$s.Length).CopyTo($footer, 0)
[System.Text.Encoding]::ASCII.GetBytes('BLURZIP1').CopyTo($footer, 8)
$fs.Write($footer, 0, 16)
$fs.Close()
Write-Host "wrote $OutFile ($([math]::Round((Get-Item $OutFile).Length/1MB,1)) MB)"
