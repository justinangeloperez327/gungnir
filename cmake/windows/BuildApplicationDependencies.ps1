param([Parameter(Mandatory = $true)][string]$WorkDir)
$ErrorActionPreference = 'Stop'
$manifest = (Resolve-Path (Join-Path $PSScriptRoot '../application-sdk')).Path
$baseline = (Get-Content (Join-Path $manifest 'vcpkg.json') -Raw | ConvertFrom-Json).'builtin-baseline'
$tool = Join-Path $WorkDir 'vcpkg'
$installed = Join-Path $WorkDir 'installed'
New-Item $WorkDir -ItemType Directory -Force | Out-Null
if (-not (Test-Path $tool)) {
    & git init $tool
    if ($LASTEXITCODE -ne 0) { throw 'Unable to create the dependency work directory' }
    & git -C $tool remote add origin https://github.com/microsoft/vcpkg.git
    if ($LASTEXITCODE -ne 0) { throw 'Unable to configure the dependency source' }
    & git -C $tool fetch --depth 1 origin $baseline
    if ($LASTEXITCODE -ne 0) { throw 'Unable to obtain the pinned vcpkg source' }
    & git -C $tool checkout --detach $baseline
    if ($LASTEXITCODE -ne 0) { throw 'Unable to select the pinned vcpkg source' }
}
$actual = (& git -C $tool rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $actual -ne $baseline) {
    throw 'Dependency work directory has a different vcpkg revision; use an empty WorkDir'
}
& (Join-Path $tool 'bootstrap-vcpkg.bat') -disableMetrics
if ($LASTEXITCODE -ne 0) { throw 'vcpkg bootstrap failed' }
& (Join-Path $tool 'vcpkg.exe') install --triplet x64-gungnir-windows `
    "--x-manifest-root=$manifest" "--x-install-root=$installed" `
    "--overlay-triplets=$(Join-Path $manifest 'triplets')" --clean-after-build --disable-metrics
if ($LASTEXITCODE -ne 0) { throw 'Static application dependency build failed' }
$prefix = Join-Path $installed 'x64-gungnir-windows'
if ($env:GITHUB_ENV) {
    "GUNGNIR_APPLICATION_DEPS=$prefix" | Out-File $env:GITHUB_ENV -Encoding utf8 -Append
}
Write-Host "Static SQLite/OpenSSL application dependencies: $prefix"
