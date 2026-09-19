[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$Root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)]
        [string]$File,
        [Parameter(Mandatory = $true)]
        [string[]]$Args
    )

    & $File @Args
    if ($LASTEXITCODE -ne 0) {
        throw "'$File $($Args -join ' ')' failed with exit code $LASTEXITCODE"
    }
}

if (-not (Get-Command xmake -ErrorAction SilentlyContinue)) {
    throw '`xmake` not found in PATH'
}

Push-Location $Root
try {
    foreach ($config in @('debug', 'release')) {
        Write-Host "======> Configure $config"
        Invoke-Native xmake @('f', '-m', $config, '-y')

        Write-Host '======> Build'
        Invoke-Native xmake @('build')

        Write-Host '======> Test'
        Invoke-Native xmake @('test')
    }

    Write-Host '======> Configure release (static CRT)'
    Invoke-Native xmake @('f', '-m', 'release', '--runtimes=MT', '-y')

    Write-Host '======> Build'
    Invoke-Native xmake @('build')

    Write-Host '======> Test'
    Invoke-Native xmake @('test')
}
finally {
    Invoke-Native xmake @('f', '-m', 'release', '-y')
    Pop-Location
}