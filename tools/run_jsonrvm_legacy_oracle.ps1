param(
    [Parameter(Mandatory = $true)]
    [string]$Executable,

    [string]$FixtureDirectory = "compat/jsonrvm-legacy-fixtures"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$PinnedCommit = "843b3326141e090ccd1a106ba0a4a21ce72805b7"
$ExpectedVersionLine = "jsonRVM version 3.0.0"

function Get-NormalizedJson {
    param([Parameter(Mandatory = $true)][string]$JsonText)

    return ($JsonText | ConvertFrom-Json | ConvertTo-Json -Compress -Depth 100)
}

function Invoke-LegacyCase {
    param(
        [Parameter(Mandatory = $true)][string]$CaseId,
        [Parameter(Mandatory = $true)][string]$FixtureName,
        [Parameter(Mandatory = $true)][string]$ExpectedJson
    )

    $fixture = (Resolve-Path (Join-Path $FixtureDirectory $FixtureName)).Path
    $entryPoint = [System.IO.Path]::GetFileNameWithoutExtension($FixtureName)
    $work = Join-Path $env:RUNNER_TEMP ("avm-jsonrvm-oracle-" + $entryPoint)
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $work | Out-Null
    Copy-Item $fixture (Join-Path $work ($entryPoint + ".json"))

    $stdoutPath = Join-Path $work "stdout.txt"
    $stderrPath = Join-Path $work "stderr.txt"

    Push-Location $work
    try {
        $process = Start-Process -FilePath $Executable -ArgumentList $entryPoint -Wait -PassThru -NoNewWindow `
            -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    }
    finally {
        Pop-Location
    }

    if ($process.ExitCode -ne 0) {
        $stderrText = Get-Content $stderrPath -Raw -ErrorAction SilentlyContinue
        throw "$CaseId: legacy jsonRVM завершился с кодом $($process.ExitCode): $stderrText"
    }

    $stdoutLines = @(Get-Content $stdoutPath)
    if ($stdoutLines.Count -lt 2 -or $stdoutLines[0].Trim() -ne $ExpectedVersionLine) {
        throw "$CaseId: неожиданный заголовок stdout legacy jsonRVM"
    }

    $jsonText = ($stdoutLines | Select-Object -Skip 1) -join "`n"
    $actual = Get-NormalizedJson $jsonText
    $expected = Get-NormalizedJson $ExpectedJson
    if ($actual -ne $expected) {
        throw "$CaseId: ожидалось $expected, получено $actual"
    }

    Write-Host "$CaseId OK: $actual"
}

function Invoke-MissingReferenceCase {
    param(
        [Parameter(Mandatory = $true)][string]$FixtureName,
        [Parameter(Mandatory = $true)][string]$ReferenceMarker
    )

    $caseId = "CASE-MISSING-REFERENCE"
    $fixture = (Resolve-Path (Join-Path $FixtureDirectory $FixtureName)).Path
    $entryPoint = [System.IO.Path]::GetFileNameWithoutExtension($FixtureName)
    $work = Join-Path $env:RUNNER_TEMP ("avm-jsonrvm-oracle-" + $entryPoint)
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Path $work | Out-Null
    Copy-Item $fixture (Join-Path $work ($entryPoint + ".json"))

    if (Test-Path (Join-Path $work ($ReferenceMarker + ".json"))) {
        throw "$caseId: marker-файл неожиданно существует"
    }

    $stdoutPath = Join-Path $work "stdout.txt"
    $stderrPath = Join-Path $work "stderr.txt"

    Push-Location $work
    try {
        $process = Start-Process -FilePath $Executable -ArgumentList $entryPoint -Wait -PassThru -NoNewWindow `
            -RedirectStandardOutput $stdoutPath -RedirectStandardError $stderrPath
    }
    finally {
        Pop-Location
    }

    if ($process.ExitCode -eq 0) {
        throw "$caseId: missing reference неожиданно завершился успешно"
    }

    $stderrText = Get-Content $stderrPath -Raw
    if ([string]::IsNullOrWhiteSpace($stderrText)) {
        throw "$caseId: legacy jsonRVM не выдал диагностический JSON"
    }

    # stderr должен оставаться валидным JSON diagnostic старого runtime.
    $null = $stderrText | ConvertFrom-Json
    if ($stderrText -notmatch [regex]::Escape($ReferenceMarker)) {
        throw "$caseId: diagnostic не содержит identity отсутствующей ссылки '$ReferenceMarker'"
    }

    Write-Host "$caseId OK: exit=$($process.ExitCode), diagnostic содержит $ReferenceMarker"
}

if (-not (Test-Path $Executable)) {
    throw "Не найден legacy jsonRVM executable: $Executable"
}

Write-Host "Legacy oracle source commit: $PinnedCommit"

Invoke-LegacyCase `
    -CaseId "CASE-ARITHMETIC" `
    -FixtureName "arithmetic-add.json" `
    -ExpectedJson '{"result":2}'

Invoke-LegacyCase `
    -CaseId "CASE-SEQUENCE-ORDER" `
    -FixtureName "sequence-order.json" `
    -ExpectedJson '{"result":3}'

Invoke-LegacyCase `
    -CaseId "CASE-FOREACH-CONTEXT" `
    -FixtureName "foreach-object-context.json" `
    -ExpectedJson '{"result":[1,2,3]}'

Invoke-LegacyCase `
    -CaseId "CASE-BOOLEAN-BRANCH" `
    -FixtureName "boolean-branch.json" `
    -ExpectedJson '{"result":42}'

Invoke-MissingReferenceCase `
    -FixtureName "missing-reference.json" `
    -ReferenceMarker "__avm_missing_reference_oracle__"

Write-Host "Legacy jsonRVM oracle: все deterministic cases подтверждены pinned runtime."
