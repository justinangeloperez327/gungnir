param([Parameter(Mandatory = $true)][string]$WorkDir)
$ErrorActionPreference = 'Stop'

# Official portable distribution; avoid Chocolatey's separate package feed.
# SHA-256 published at https://sourceforge.net/projects/nsis/files/NSIS%203/3.13/nsis-3.13.zip/download
$version = '3.13'
$expectedHash = 'ba63dffc4410ee89193e1cb5a41989991bd77c61068da17e3156d136b7b0b3d8'
New-Item $WorkDir -ItemType Directory -Force | Out-Null
$archive = Join-Path $WorkDir "nsis-$version.zip"
$downloaded = $false
foreach ($url in @(
    "https://downloads.sourceforge.net/project/nsis/NSIS%203/$version/nsis-$version.zip",
    "https://sourceforge.net/projects/nsis/files/NSIS%203/$version/nsis-$version.zip/download"
)) {
    try {
        Invoke-WebRequest -Uri $url -OutFile $archive -MaximumRetryCount 3 -RetryIntervalSec 2 -TimeoutSec 180
        $downloaded = $true
        break
    } catch {
        Write-Warning "NSIS download failed from ${url}: $($_.Exception.Message)"
    }
}
if (-not $downloaded) { throw 'Unable to download the pinned NSIS distribution' }
if ((Get-FileHash $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $expectedHash) {
    throw 'NSIS archive SHA-256 mismatch; refusing to extract or execute it'
}

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
