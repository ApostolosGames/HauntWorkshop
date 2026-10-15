<#
Packages one HAUNT Workshop map into <Stem>.pak / .utoc / .ucas that hold only the map.

A standard Unreal package is a whole standalone game: engine .ini files, every plugin descriptor, the .uproject,
the global shader cache, Slate and internationalization data. HAUNT refuses those. This script cooks normally,
then rebuilds the pak and IoStore container from UAT's own file lists, keeping only:
  - the packages the map depends on (your plugin plus the engine assets it references)
  - AssetRegistry.bin, your plugin's .uplugin, and your shader library

It also checks your renderer settings against HAUNT's and records them in the .uplugin first, and it never
builds C++, so engine plugins you enabled or disabled don't force a code build.

Usage (save everything in the editor first):
  powershell -ExecutionPolicy Bypass -File Tools\PackageWorkshopMap.ps1 `
      -Project "C:\Maps\MyMapProject\MyMapProject.uproject" -Plugin MyName_MyMap -Map /MyName_MyMap/Maps/MyMap

Needs Python 3 on PATH, and the PythonScriptPlugin + EditorScriptingUtilities plugins (stock engine plugins;
the script enables them for its own run only).
#>
param(
    [Parameter(Mandatory)] [string]$Project,
    [Parameter(Mandatory)] [string]$Plugin,
    [Parameter(Mandatory)] [string]$Map,
    [string]$EngineDir = "C:\Program Files\Epic Games\UE_5.8",
    [string]$Stem = $Plugin,
    [string]$OutputDir = "",
    # Editor-only plugins whose cook-time errors fail the cook. They are never needed by a cooked map.
    [string]$DisablePlugins = "AllToolsets,ModelContextProtocol"
)
$ErrorActionPreference = "Stop"

$Project = (Resolve-Path $Project).Path
$ProjectDir = Split-Path -Parent $Project
$ProjectName = [IO.Path]::GetFileNameWithoutExtension($Project)
if (-not (Test-Path (Join-Path $EngineDir "Engine"))) { throw "-EngineDir '$EngineDir' is not a UE 5.8 install (no Engine folder)." }
$Engine = Join-Path $EngineDir "Engine"
if (-not $OutputDir) { $OutputDir = Join-Path $ProjectDir "Workshop\Upload" }
if ($Stem -match "_P$") { throw "Stem '$Stem' ends in _P, which HAUNT reserves for patches." }

$UECmd = Join-Path $Engine "Binaries\Win64\UnrealEditor-Cmd.exe"
$UnrealPak = Join-Path $Engine "Binaries\Win64\UnrealPak.exe"
$Work = Join-Path $ProjectDir "Saved\WorkshopPak"
$StageDir = Join-Path $ProjectDir "Saved\WorkshopStage"

# 1. Renderer settings: refuse to cook with settings HAUNT can't render, and record them in the .uplugin.
python (Join-Path $PSScriptRoot "haunt_renderer_settings.py") stamp $Project --engine $EngineDir --plugin $Plugin
if ($LASTEXITCODE -ne 0) { throw "Renderer settings don't match HAUNT's; see the list above." }

# 2. Cook and stage as usual. -skipbuild/-nocompile: content only, no C++ build.
foreach ($dir in $Work, $StageDir) { if (Test-Path $dir) { Remove-Item -Recurse -Force $dir } }
New-Item -ItemType Directory (Join-Path $Work "out") | Out-Null

& (Join-Path $Engine "Build\BatchFiles\RunUAT.bat") BuildCookRun `
    "-project=$Project" -noP4 -platform=Win64 -clientconfig=Shipping `
    -cook "-map=$Map" -stage -pak -iostore -compressed `
    -skipbuild -nocompile -nocompileeditor -nocompileuat -nodebuginfo `
    "-unrealexe=$UECmd" "-AdditionalCookerOptions=-DisablePlugins=$DisablePlugins" `
    "-stagingdirectory=$StageDir" -unattended -utf8output
if ($LASTEXITCODE -ne 0) { throw "BuildCookRun failed ($LASTEXITCODE)" }

# 3. Everything the map depends on.
$DepsFile = Join-Path $Work "deps.txt"
$env:HAUNT_MAP = $Map
$env:HAUNT_DEPS_OUT = $DepsFile
& $UECmd $Project -run=pythonscript "-script=$(Join-Path $PSScriptRoot 'MapDependencies.py')" `
    "-EnablePlugins=PythonScriptPlugin,EditorScriptingUtilities" "-DisablePlugins=$DisablePlugins" `
    -unattended -nosplash -nopause | Out-Null
if (-not (Test-Path $DepsFile)) { throw "The map's dependency list was not written ($DepsFile)." }
$Deps = New-Object 'System.Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
Get-Content $DepsFile | Where-Object { $_ } | ForEach-Object { [void]$Deps.Add($_.Trim()) }
if (-not $Deps.Contains($Map)) { throw "'$Map' is not a map in this project." }

# 4. Keep only the map's files from UAT's response files.
function Get-StagedPath([string]$Line) {
    if ($Line -match '"[^"]*"\s+"([^"]+)"') { return $Matches[1] -replace '^(\.\./)+', '' }
    return $null
}
function Get-PackageName([string]$Staged) {
    $noExt = $Staged -replace '\.[^./]+$', ''
    if ($noExt -match '^Engine/Content/(.+)$') { return "/Engine/$($Matches[1])" }
    if ($noExt -match '/Plugins/(?:.+/)?([^/]+)/Content/(.+)$') { return "/$($Matches[1])/$($Matches[2])" }
    if ($noExt -match '^[^/]+/Content/(.+)$') { return "/Game/$($Matches[1])" }
    return $null
}

$Responses = Join-Path $Engine "Programs\AutomationTool\Saved\ResponseFiles"
$Descriptor = "^$ProjectName/Plugins/(.+/)?$Plugin/$Plugin\.uplugin$"
$PakLines = Get-Content (Join-Path $Responses "PakList_$ProjectName-Windows.txt") | Where-Object {
    $s = Get-StagedPath $_
    $s -and ($s -eq "$ProjectName/AssetRegistry.bin" -or $s -match $Descriptor -or $s -match "^$ProjectName/Content/ShaderTypeInfo-$ProjectName-")
}
$IoLines = Get-Content (Join-Path $Responses "PakListIoStore_$ProjectName-Windows.txt") | Where-Object {
    $s = Get-StagedPath $_
    $s -and ($s -match "^$ProjectName/Content/ShaderArchive-$ProjectName-" -or $Deps.Contains([string](Get-PackageName $s)))
}
$PakResp = Join-Path $Work "PakList.txt"; $IoResp = Join-Path $Work "PakListIoStore.txt"
Set-Content -Path $PakResp -Value $PakLines -Encoding UTF8
Set-Content -Path $IoResp -Value $IoLines -Encoding UTF8
Write-Host "Map-only pak: $($PakLines.Count) loose files, $($IoLines.Count) IoStore entries"

# 5. Build the pak and the IoStore container.
$Cooked = Join-Path $ProjectDir "Saved\Cooked\Windows"
$Meta = Join-Path $Cooked "$ProjectName\Metadata"
$Container = Join-Path $Work "out\$ProjectName-Windows"
$Common = @("-patchpaddingalign=2048", "-compressionformats=Oodle", "-compresslevel=4", "-compressmethod=Kraken",
            "-cryptokeys=$(Join-Path $Meta 'Crypto.json')", "-platform=Windows", "-unattended")
$PakCmds = Join-Path $Work "PakCommands.txt"
Set-Content -Path $PakCmds -Encoding UTF8 -Value "`"$Container.pak`" -create=`"$PakResp`""
& $UnrealPak $Project @Common "-CreateMultiple=$PakCmds"
if ($LASTEXITCODE -ne 0) { throw "UnrealPak (pak) failed ($LASTEXITCODE)" }
$IoCmds = Join-Path $Work "IoStoreCommands.txt"
Set-Content -Path $IoCmds -Encoding UTF8 -Value "-Output=`"$Container.utoc`" -ContainerName=$ProjectName-Windows -ResponseFile=`"$IoResp`""
& $UnrealPak $Project $Project "-CreateGlobalContainer=$(Join-Path $Work 'out\global.utoc')" `
    "-PackageStoreManifest=$(Join-Path $Meta 'packagestore.manifest')" "-CookedDirectory=$Cooked" `
    "-Commands=$IoCmds" "-ScriptObjects=$(Join-Path $Meta 'scriptobjects.bin')" @Common `
    -compressionMinBytesSaved=1024 -compressionMinPercentSaved=5 -WriteBackMetadataToAssetRegistry=Disabled
if ($LASTEXITCODE -ne 0) { throw "UnrealPak (IoStore) failed ($LASTEXITCODE)" }

# 6. The upload folder holds only the three files.
if (Test-Path $OutputDir) { Remove-Item -Recurse -Force $OutputDir }
New-Item -ItemType Directory $OutputDir | Out-Null
foreach ($ext in "pak", "utoc", "ucas") { Copy-Item "$Container.$ext" (Join-Path $OutputDir "$Stem.$ext") }

# 7. Check the result the way HAUNT will.
$List = (& $UnrealPak (Join-Path $OutputDir "$Stem.pak") -List) + (& $UnrealPak (Join-Path $OutputDir "$Stem.utoc") -List)
$Maps = @($List | Select-String -Pattern '\.umap"' | Select-Object -ExpandProperty Line)
$Problems = @()
if (-not ($List | Select-String -Pattern "AssetRegistry\.bin")) { $Problems += "AssetRegistry.bin is missing: check [Pak] DirectoryIndexKeepFiles in DefaultGame.ini." }
if ($Maps.Count -ne 1) { $Problems += "Expected exactly one .umap, found $($Maps.Count)." }
if ($List | Select-String -Pattern "$ProjectName/Content/.*\.(uasset|umap)") { $Problems += "Assets under /Game/ were cooked; keep everything the map uses in the '$Plugin' plugin." }
if (-not ($List | Select-String -Pattern "ShaderArchive-.*PCD3D_SM6")) { $Problems += "No SM6 shader archive: copy the WindowsTargetSettings section from Templates/Config/DefaultEngine.ini." }
if (-not ($List | Select-String -Pattern "ShaderArchive-.*PCD3D_SM5")) { $Problems += "No SM5 shader archive: copy the WindowsTargetSettings section from Templates/Config/DefaultEngine.ini." }

Write-Host "`n==== Workshop package ===="
Get-ChildItem $OutputDir | ForEach-Object { "{0,-28} {1,8:N1} MB" -f $_.Name, ($_.Length / 1MB) } | Write-Host
$Maps | Write-Host
if ($Problems) {
    $Problems | ForEach-Object { Write-Warning $_ }
    throw "The package has problems HAUNT will refuse; fix them and run again."
}
Write-Host "Ready to upload: $OutputDir"
