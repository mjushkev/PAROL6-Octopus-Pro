[CmdletBinding()]
param([string]$Destination = "")

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $Destination) { $Destination = Join-Path $projectRoot ".cache\upstreams" }
New-Item -ItemType Directory -Force -Path $Destination | Out-Null

$sources = @(
    @{ Name="waldo-commander"; Url="https://github.com/Jepson2k/Waldo-Commander.git"; Revision="d5acbe1bea86cf1f207b8e912b8e36f9d7dbaf91" },
    @{ Name="parol6-python-api"; Url="https://github.com/PCrnjak/PAROL6-python-API.git"; Revision="829c2c73051c18d9cbf2e4cb07508a1557f63294" },
    @{ Name="waldoctl"; Url="https://github.com/Jepson2k/waldoctl.git"; Revision="9ceab01e9b43495f4115cda90d26563220a1466a" }
)

foreach ($source in $sources) {
    $target = Join-Path $Destination $source.Name
    if (-not (Test-Path -LiteralPath (Join-Path $target ".git"))) {
        git clone --filter=blob:none --no-checkout $source.Url $target
        if ($LASTEXITCODE -ne 0) { throw "Clone failed for $($source.Name)." }
    }
    git -C $target fetch --filter=blob:none origin $source.Revision
    if ($LASTEXITCODE -ne 0) { throw "Fetch failed for $($source.Name)." }
    git -C $target checkout --detach $source.Revision
    if ($LASTEXITCODE -ne 0) { throw "Checkout failed for $($source.Name)." }
    $actual = (git -C $target rev-parse HEAD).Trim()
    if ($actual -ne $source.Revision) { throw "Revision mismatch for $($source.Name): $actual" }
    Write-Host "$($source.Name): $actual"
}

