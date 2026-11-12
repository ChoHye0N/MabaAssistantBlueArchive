# SPDX-License-Identifier: AGPL-3.0-only
# SPDX-FileCopyrightText: 2026 MabaAssistantBlueArchive contributors
#
# Builds the Windows release archive from a finished Core build and an OCR environment (ocr/.venv
# with its models). Nothing to install on the target PC: self-contained GUI, embeddable Python.
# Layout: the executable and the legal texts on top; bin/ native libraries and the command line tool;
# data/ what the program reads and tools/ that work on it; ocr/ Python and models.
param(
    [Parameter(Mandatory)] [string] $Version,
    [string] $OutDir = 'dist',
    [string] $Repository = '',  # owner/name: turns on the new-version notice
    [string] $PythonVersion = '3.12.10'
)
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
Set-Location $repo
New-Item -ItemType Directory -Force $OutDir | Out-Null
$out = (Resolve-Path $OutDir).Path
$name = "MABA-$Version-win-x64"
$pkg = Join-Path $out $name
if (Test-Path $pkg) { Remove-Item -Recurse -Force $pkg }
if (Test-Path "$pkg.zip") { Remove-Item -Force "$pkg.zip" }
New-Item -ItemType Directory -Force $pkg | Out-Null

# GUI with its own .NET runtime, plus MabaCore and the OpenCV DLLs
$publish = Join-Path $out 'publish'
if (Test-Path $publish) { Remove-Item -Recurse -Force $publish }
& dotnet publish gui/Maba.Gui -c Release -r win-x64 --self-contained true -o $publish -v quiet "-p:Version=$Version" "-p:MabaRepository=$Repository" `
    -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true -p:DebugType=none
if ($LASTEXITCODE -ne 0) { throw 'dotnet publish failed' }
Copy-Item "$publish\MabaAssistantBlueArchive.exe" $pkg
New-Item -ItemType Directory -Force "$pkg\bin" | Out-Null
Copy-Item "$publish\*.dll" "$pkg\bin"
Copy-Item build/windows-msvc/core/Release/maba_cli.exe "$pkg\bin"

# Data and tools
$data = "$pkg\data"
New-Item -ItemType Directory -Force $data, "$data\assets", "$data\assets-local\ko", "$data\assets-local\common", "$data\tools", "$pkg\ocr" | Out-Null
foreach ($d in 'config', 'pages', 'popups', 'tasks', 'strings') { Copy-Item -Recurse $d "$data\$d" }
Copy-Item assets\README.md, assets\PROVENANCE.md "$data\assets"
Copy-Item assets\common, assets\ko "$data\assets" -Recurse
Set-Content -Path "$data\assets-local\README.txt" -Encoding utf8 -Value 'Templates you capture yourself go here (ko\ for Korean text, common\ for icons). They take precedence over assets\. See tools\README.md.'
Copy-Item tools\template_capture.py, tools\check_templates.py, tools\requirements.txt, tools\README.md "$data\tools"
Copy-Item LICENSE, THIRD_PARTY_NOTICES.md, README.md, README.en.md $pkg
New-Item -ItemType Directory -Force "$pkg\docs\images" | Out-Null
Copy-Item docs\images\screenshot-ko.png, docs\images\logo.png "$pkg\docs\images"

# OCR: embeddable Python with the packages and models of ocr/.venv
Copy-Item ocr\server.py, ocr\requirements.txt, ocr\README.md "$pkg\ocr"
$embed = Join-Path $out "python-$PythonVersion-embed-amd64.zip"
if (-not (Test-Path $embed)) {
    Invoke-WebRequest -UseBasicParsing "https://www.python.org/ftp/python/$PythonVersion/python-$PythonVersion-embed-amd64.zip" -OutFile $embed
}
Expand-Archive $embed -DestinationPath "$pkg\ocr\python" -Force
$pth = (Get-ChildItem "$pkg\ocr\python" -Filter 'python*._pth' | Select-Object -First 1).FullName
$zipName = (Get-ChildItem "$pkg\ocr\python" -Filter 'python3*.zip' | Select-Object -First 1).Name
Set-Content -Path $pth -Encoding ascii -Value @($zipName, '.', 'Lib\site-packages', 'import site')
$site = "$pkg\ocr\python\Lib\site-packages"
New-Item -ItemType Directory -Force $site | Out-Null
Get-ChildItem ocr\.venv\Lib\site-packages |
    Where-Object { $_.Name -notmatch '^(pip|setuptools|_distutils_hack|distutils-precedence\.pth)' -and $_.Name -notlike 'pip-*' } |
    Copy-Item -Destination $site -Recurse
Get-ChildItem $site -Recurse -Directory -Filter __pycache__ | Remove-Item -Recurse -Force

Compress-Archive -Path "$pkg\*" -DestinationPath "$pkg.zip" -CompressionLevel Optimal
Write-Output "package: $pkg.zip ($([math]::Round((Get-Item "$pkg.zip").Length / 1MB, 1)) MB)"
