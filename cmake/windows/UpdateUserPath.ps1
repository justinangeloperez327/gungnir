[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $BinPath,

    [Parameter(Mandatory = $true)]
    [ValidateSet("Add", "Remove")]
    [string] $Action
)

$ErrorActionPreference = "Stop"

function Normalize-PathEntry {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Value
    )

    $trimmed = $Value.Trim().Trim('"')
    if ([string]::IsNullOrWhiteSpace($trimmed)) {
        return ""
    }

    $expanded = [Environment]::ExpandEnvironmentVariables($trimmed)

    try {
        $expanded = [IO.Path]::GetFullPath($expanded)
    }
    catch {
        # Preserve unusual but valid PATH entries while still comparing
        # ordinary filesystem paths consistently.
    }

    return $expanded.TrimEnd('\')
}

$target = Normalize-PathEntry $BinPath
if ([string]::IsNullOrWhiteSpace($target)) {
    throw "Gungnir bin path cannot be empty."
}

$current = [Environment]::GetEnvironmentVariable(
    "Path",
    [EnvironmentVariableTarget]::User
)

$entries = [Collections.Generic.List[string]]::new()

if (-not [string]::IsNullOrWhiteSpace($current)) {
    foreach ($entry in ($current -split ';')) {
        if (-not [string]::IsNullOrWhiteSpace($entry)) {
            $entries.Add($entry)
        }
    }
}

$matchesTarget = {
    param([string] $Entry)

    $normalized = Normalize-PathEntry $Entry
    return [StringComparer]::OrdinalIgnoreCase.Equals(
        $normalized,
        $target
    )
}

if ($Action -eq "Add") {
    $exists = $false

    foreach ($entry in $entries) {
        if (& $matchesTarget $entry) {
            $exists = $true
            break
        }
    }

    if (-not $exists) {
        $entries.Add($BinPath.TrimEnd('\'))
    }
}
else {
    $filtered = [Collections.Generic.List[string]]::new()

    foreach ($entry in $entries) {
        if (-not (& $matchesTarget $entry)) {
            $filtered.Add($entry)
        }
    }

    $entries = $filtered
}

$newPath = [string]::Join(';', $entries)

if ([string]::IsNullOrEmpty($newPath)) {
    [Environment]::SetEnvironmentVariable(
        "Path",
        $null,
        [EnvironmentVariableTarget]::User
    )
}
else {
    [Environment]::SetEnvironmentVariable(
        "Path",
        $newPath,
        [EnvironmentVariableTarget]::User
    )
}
