[CmdletBinding()]
param(
    [string]$Python = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot

if (-not $Python) {
    $localPython = Join-Path $projectRoot ".venv\Scripts\python.exe"
    if (Test-Path -LiteralPath $localPython) {
        $Python = $localPython
    } else {
        $command = Get-Command python -ErrorAction SilentlyContinue
        if ($command) {
            $Python = $command.Source
        } else {
            throw "Python 3.12+ was not found. Pass -Python with an explicit interpreter path."
        }
    }
}

& $Python (Join-Path $projectRoot "tools\protocol_analyzer\generate_vectors.py")
if ($LASTEXITCODE -ne 0) { throw "Protocol generation failed." }

Push-Location $projectRoot
try {
    & $Python -m unittest discover -s tests -v
    if ($LASTEXITCODE -ne 0) { throw "Test suite failed." }
} finally {
    Pop-Location
}

