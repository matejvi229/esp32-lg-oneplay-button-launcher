param(
  [string]$SketchPath = ".\esp32_test",
  [string]$Fqbn = "esp32:esp32:esp32s3",
  [string]$Port = "COM7",
  [string]$BoardOptions = "FlashSize=8M,PSRAM=disabled,CDCOnBoot=cdc",
  [string]$UploadSpeed = "115200",
  [switch]$CompileOnly
)

$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
$cli = Join-Path $repoRoot ".tools\arduino-cli\arduino-cli.exe"

if (-not (Test-Path $cli)) {
  throw "arduino-cli not found at: $cli"
}

$resolvedSketch = Resolve-Path $SketchPath
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$buildPath = Join-Path $repoRoot ("build\script_" + $timestamp)
New-Item -ItemType Directory -Force -Path $buildPath | Out-Null

Write-Host "Compiling..."
Write-Host "  Sketch: $resolvedSketch"
Write-Host "  FQBN:   $Fqbn"
Write-Host "  Build:  $buildPath"

& $cli compile `
  --fqbn $Fqbn `
  --board-options $BoardOptions `
  --build-path $buildPath `
  $resolvedSketch

if ($CompileOnly) {
  Write-Host "Compile finished. Upload skipped (-CompileOnly)."
  exit 0
}

Write-Host "Uploading..."
Write-Host "  Port:   $Port"

& $cli upload `
  -p $Port `
  --fqbn $Fqbn `
  --upload-property ("upload.speed=" + $UploadSpeed) `
  --build-path $buildPath `
  $resolvedSketch

Write-Host "Done."
