param([Parameter(Mandatory = $true)][string]$WorkDir)
$ErrorActionPreference = 'Stop'

# Official portable distribution; avoid Chocolatey's separate package feed.
# SHA-256 published at https://sourceforge.net/projects/nsis/files/NSIS%203/3.13/nsis-3.13.zip/download
$version = '3.13'
$expectedHash = 'ba63dffc4410ee89193e1cb5a41989991bd77c61068da17e3156d136b7b0b3d8'
New-Item $WorkDir -ItemType Directory -Force | Out-Null
$archive = Join-Path $WorkDir "nsis-$version.zip"
. (Join-Path $PSScriptRoot 'DownloadVerifiedArchive.ps1')
# Tauri mirrors the byte-identical official archive on GitHub. Keep the
# upstream checksum authoritative for every source, including the mirror.
Get-VerifiedArchive -Destination $archive -ExpectedHash $expectedHash -Urls @(
    "https://github.com/tauri-apps/binary-releases/releases/download/nsis-$version/nsis-$version.zip",
    "https://downloads.sourceforge.net/project/nsis/NSIS%203/$version/nsis-$version.zip",
    "https://sourceforge.net/projects/nsis/files/NSIS%203/$version/nsis-$version.zip/download"
)

Expand-Archive -Path $archive -DestinationPath $WorkDir -Force
$nsis = Join-Path $WorkDir "nsis-$version"
$compiler = Join-Path $nsis 'makensis.exe'
if (-not (Test-Path $compiler)) { throw 'Verified NSIS archive is missing makensis.exe' }
$actualVersion = (& $compiler /VERSION | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or $actualVersion -ne "v$version") {
    throw "Unexpected NSIS compiler version: $actualVersion"
}
$env:NSISDIR = $nsis
$env:PATH = "$nsis;$env:PATH"
if ($env:GITHUB_PATH) {
    $nsis | Out-File -FilePath $env:GITHUB_PATH -Encoding utf8 -Append
}
if ($env:GITHUB_ENV) {
    "NSISDIR=$nsis" | Out-File -FilePath $env:GITHUB_ENV -Encoding utf8 -Append
}
Write-Host "Using NSIS $actualVersion from $nsis"
