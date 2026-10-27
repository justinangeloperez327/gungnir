function Get-VerifiedArchive {
    param(
        [Parameter(Mandatory = $true)][string[]]$Urls,
        [Parameter(Mandatory = $true)][string]$Destination,
        [Parameter(Mandatory = $true)][string]$ExpectedHash
    )
    foreach ($url in $Urls) {
        try {
            Remove-Item $Destination -Force -ErrorAction SilentlyContinue
            Invoke-WebRequest -Uri $url -OutFile $Destination -MaximumRetryCount 3 -RetryIntervalSec 2 -TimeoutSec 180
            $actualHash = (Get-FileHash $Destination -Algorithm SHA256).Hash.ToLowerInvariant()
            if ($actualHash -ne $ExpectedHash) {
                $size = (Get-Item $Destination).Length
                throw "SHA-256 mismatch ($size bytes): expected $ExpectedHash, received $actualHash"
            }
            Write-Host "Verified archive from $url"
            return
        } catch {
            Write-Warning "Archive download rejected from ${url}: $($_.Exception.Message)"
            Remove-Item $Destination -Force -ErrorAction SilentlyContinue
        }
    }
    throw 'All archive sources failed verification; refusing to extract or execute any download'
}
