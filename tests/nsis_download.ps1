$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../cmake/windows/DownloadVerifiedArchive.ps1')
$work = Join-Path ([System.IO.Path]::GetTempPath()) ([guid]::NewGuid().ToString())
New-Item $work -ItemType Directory | Out-Null
try {
    $destination = Join-Path $work 'archive.zip'
    $fixture = Join-Path $work 'fixture'
    [System.IO.File]::WriteAllText($fixture, 'verified fixture')
    $hash = (Get-FileHash $fixture -Algorithm SHA256).Hash.ToLowerInvariant()
    $script:requests = @()
    # Mock downloads only; hashing and failed-file cleanup remain real.
    function Invoke-WebRequest {
        param($Uri, $OutFile, $MaximumRetryCount, $RetryIntervalSec, $TimeoutSec)
        $script:requests += $Uri
        switch ($Uri) {
            'network-error' { throw 'HTTP 503' }
            'html-response' { [System.IO.File]::WriteAllText($OutFile, '<html>unavailable</html>') }
            'valid' { Copy-Item $fixture $OutFile }
            default { throw "Unexpected test URL: $Uri" }
        }
    }
    Get-VerifiedArchive -Destination $destination -ExpectedHash $hash -Urls @('network-error', 'html-response', 'valid')
    if (($script:requests -join ',') -ne 'network-error,html-response,valid') { throw 'Fallback did not try every rejected source' }
    if ((Get-FileHash $destination -Algorithm SHA256).Hash.ToLowerInvariant() -ne $hash) { throw 'Verified file was not retained' }
    $script:requests = @()
    Get-VerifiedArchive -Destination $destination -ExpectedHash $hash -Urls @('valid', 'network-error')
    if (($script:requests -join ',') -ne 'valid') { throw 'Download continued after successful verification' }
    $rejected = $false
    try { Get-VerifiedArchive -Destination $destination -ExpectedHash $hash -Urls @('html-response', 'network-error') }
    catch { $rejected = $true }
    if (-not $rejected -or (Test-Path $destination)) { throw 'Unverified download was accepted or left behind' }
    Write-Host 'NSIS download fallback, checksum rejection, and cleanup tests passed'
} finally {
    Remove-Item $work -Recurse -Force
}
