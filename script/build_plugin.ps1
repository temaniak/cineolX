param([int]$Jobs = 6)
$ErrorActionPreference = 'Stop'
$TaskRoot = Split-Path $PSScriptRoot -Parent
$TaskBuild = Join-Path $TaskRoot 'build/windows'

function Invoke-Checked {
    param([string]$Program, [string[]]$Arguments)
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed with exit code $LASTEXITCODE" }
}

# prepare_dependency.py applies patches only to the ignored build export.
$TaskGit = (Get-Command git -ErrorAction Stop).Source
$TaskPatchDir = Join-Path (Split-Path (Split-Path $TaskGit -Parent) -Parent) 'usr/bin'
$TaskOldPath = $env:PATH
$TaskOldCache = $env:CINEOL224_CACHE_DIR
try {
    if (Test-Path (Join-Path $TaskPatchDir 'patch.exe')) {
        $env:PATH = "$TaskPatchDir;$env:PATH"
    }
    $TaskPython = (Get-Command python -ErrorAction Stop).Source
    $TaskArgs = @('-S', $TaskRoot, '-B', $TaskBuild, '-G', 'Visual Studio 17 2022',
        '-A', 'x64', '-DCINEOL_BUILD_PLUGIN=ON', '-DNATIVE_HALL_BUILD_TOOLS=OFF',
        "-DPython3_EXECUTABLE=$TaskPython")
    if ($env:JUCE_DIR) { $TaskArgs += "-DFETCHCONTENT_SOURCE_DIR_JUCE=$env:JUCE_DIR" }
    Invoke-Checked cmake $TaskArgs
    Invoke-Checked cmake @('--build', $TaskBuild, '--config', 'Release', '--parallel',
        "$Jobs", '--target', 'NativeHall224_VST3', 'cineol_rom_import_check')
    $env:CINEOL224_CACHE_DIR = Join-Path $TaskBuild ('check-cache-' + [guid]::NewGuid())
    Invoke-Checked (Join-Path $TaskBuild 'cineol_rom_import_check_artefacts/Release/cineol_rom_import_check.exe') @('--empty')
    Write-Host "Cineol-X 224 VST3: $TaskBuild/NativeHall224_artefacts/Release/VST3/Cineol-X 224.vst3"
} finally {
    $env:PATH = $TaskOldPath
    $env:CINEOL224_CACHE_DIR = $TaskOldCache
}
