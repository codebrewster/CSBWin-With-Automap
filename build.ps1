<#
.SYNOPSIS
    Builds CSB on Windows with MinGW-w64.

.DESCRIPTION
    Builds the SDL2/Dear ImGui front end (the same sources the Unix makefile
    builds) into build\win64\csb.exe.

    The toolchain is the MinGW-w64 that ships with the Qt kit already installed
    in C:\Qt\Tools\mingw1310_64, so nothing has to be downloaded. SDL2 comes
    from third_party\SDL2-2.30.9.

.PARAMETER Clean
    Delete the object directory and the executable before building.

.PARAMETER Run
    Start the game in Dungeons\Dungeonmaster after a successful build.

.EXAMPLE
    .\build.ps1
    .\build.ps1 -Clean
    .\build.ps1 -Run
#>
[CmdletBinding()]
param(
    [switch] $Clean,
    [switch] $Run
)

$ErrorActionPreference = 'Stop'

$Root    = Split-Path -Parent $MyInvocation.MyCommand.Path
$Src     = Join-Path $Root 'src'
$Build   = Join-Path $Root 'build\win64'
$Obj     = Join-Path $Build 'obj'
$SDL     = Join-Path $Root 'third_party\SDL2-2.30.9\x86_64-w64-mingw32'
$MinGW   = 'C:\Qt\Tools\mingw1310_64\bin'

# ---------------------------------------------------------------- toolchain --

if (-not (Test-Path (Join-Path $MinGW 'g++.exe'))) {
    throw "MinGW-w64 not found in $MinGW. Install it with the Qt Maintenance Tool, or edit `$MinGW in this script."
}
if (-not (Test-Path (Join-Path $SDL 'include\SDL2\SDL.h'))) {
    throw "SDL2 not found in $SDL. See BUILDING-WINDOWS.md for how to fetch it."
}

$env:PATH = "$MinGW;$env:PATH"

# ---------------------------------------------------------------- what to build --

# The game, minus the platform mains for the other systems (CSBwin.cpp is the
# Win32 front end and CSBraspian.cpp the SDL 1.2 one; the makefile's SDL20
# target builds CSBlinux.cpp on every platform).
$CppSources = @(
    'DSA', 'data', 'Code11f52', 'Graphics', 'Magic', 'CSBCode', 'Timer', 'Character',
    'system', 'SmartDiscard', 'VBL', 'Mouse', 'Recording', 'LinCSBUI', 'CSBlinux',
    'Code51a4', 'CSBUI', 'MoveObject', 'Hint', 'Monster', 'Code17818', 'Viewport',
    'AsciiDump', 'Menu', 'Chaos', 'SaveGame', 'Translation', 'Bitmaps', 'md5C',
    'utility', 'WinScreen', 'Code13ea4', 'Codea59a', 'RC4', 'Attack', 'Sound',
    'Code1f9e6', 'Code390e', 'Code222ea', 'Statistics', 'SoundMixer',
    'AutoMapExplored', 'AutoMapKnowledge', 'AutoMapSight', 'AutoMapFile',
    'AutoMapScene', 'AutoMapPress', 'AutoMapLinkWatch', 'AutoMapDrawTally',
    'AutoMapItemMemory', 'AutoMapCellMemory', 'AutoMapPlayerNotes',
    'AutoMapMonsterDamage', 'AutoMapVisibilityCases', 'AutoMapWindow',
    'AutoMapHooks', 'AutoMapRecorder',
    'AutoMapLevelReader', 'AutoMapSprites', 'AutoMapMeasure',
    'ShotMissiles'
)

$ImguiSources = @(
    'imgui', 'imgui_draw', 'imgui_impl_sdl2', 'imgui_tables', 'imgui_widgets',
    'imgui_impl_sdlrenderer2', 'imguifilebrowser'
)

$SdlSoundSources = @('SDL_sound', 'SDL_sound_mp3', 'SDL_sound_raw', 'SDL_sound_wav')

# Compiled as C, not C++.
$CSources = @('confile.c', 'fnmatch\fnmatch.c')

# ---------------------------------------------------------------- flags --

$Defines = @('-D_LINUX', '-DRGB655', '-DSDL20', '-DMINGW')
$Includes = @(
    '-Isdl_sound', '-Iimgui', '-Ifnmatch',
    "-I$SDL\include\SDL2", "-I$SDL\include"
)

$CxxFlags = @('-O2', '-std=c++11') + $Defines + $Includes
$CFlags   = @('-O2')             + $Defines + $Includes
$LdFlags  = @("-L$SDL\lib", '-lmingw32', '-lSDL2main', '-lSDL2', '-mwindows')

# ---------------------------------------------------------------- clean --

if ($Clean) {
    if (Test-Path $Obj) { Remove-Item $Obj -Recurse -Force }
    $exe = Join-Path $Build 'csb.exe'
    if (Test-Path $exe) { Remove-Item $exe -Force }
}

foreach ($d in @($Obj, "$Obj\imgui", "$Obj\sdl_sound", "$Obj\fnmatch")) {
    New-Item -ItemType Directory -Force -Path $d | Out-Null
}

# ---------------------------------------------------------------- compile --

$Objects = New-Object System.Collections.Generic.List[string]
$Failed  = New-Object System.Collections.Generic.List[string]
$Index   = 0
$Total   = $CppSources.Count + $ImguiSources.Count + $SdlSoundSources.Count + $CSources.Count

# Timestamp of the newest header anywhere in the tree. An object older than this
# is rebuilt, which is a cheap stand-in for real dependency tracking: it never
# misses a header change, at the cost of a few needless rebuilds. The .inl files
# are included like headers (system.cpp includes LinuxFileName.inl).
$NewestHeader = Get-ChildItem $Src -Recurse -Include *.h, *.hpp, *.inl -File |
    Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty LastWriteTime

function Invoke-Compile {
    param([string] $Compiler, [string[]] $Flags, [string] $Source, [string] $Target)

    $script:Index++
    Write-Progress -Activity 'Compiling CSB' -Status $Source -PercentComplete (100 * $script:Index / $Total)

    if (Test-Path $Target) {
        $objTime = (Get-Item $Target).LastWriteTime
        if ($objTime -gt (Get-Item $Source).LastWriteTime -and $objTime -gt $NewestHeader) {
            return $Target   # up to date
        }
    }

    $output = & $Compiler @Flags -c $Source -o $Target 2>&1
    if ($LASTEXITCODE -ne 0) {
        $script:Failed.Add($Source)
        Write-Host "FAILED: $Source" -ForegroundColor Red
        $output | Where-Object { $_ -match 'error|fatal' } | Select-Object -First 5 |
            ForEach-Object { Write-Host "    $_" -ForegroundColor DarkGray }
        return $null
    }
    return $Target
}

Push-Location $Src
try {
    foreach ($name in $CppSources) {
        $o = Join-Path $Obj "$name.o"
        if (Invoke-Compile 'g++' $CxxFlags "$name.cpp" $o) { $Objects.Add($o) }
    }
    foreach ($name in $ImguiSources) {
        $o = Join-Path $Obj "imgui\$name.o"
        if (Invoke-Compile 'g++' $CxxFlags "imgui\$name.cpp" $o) { $Objects.Add($o) }
    }
    foreach ($name in $SdlSoundSources) {
        $o = Join-Path $Obj "sdl_sound\$name.o"
        if (Invoke-Compile 'gcc' $CFlags "sdl_sound\$name.c" $o) { $Objects.Add($o) }
    }
    foreach ($name in $CSources) {
        $leaf = [IO.Path]::GetFileNameWithoutExtension($name)
        $o = Join-Path $Obj "fnmatch\$leaf.o"
        if (Invoke-Compile 'gcc' $CFlags $name $o) { $Objects.Add($o) }
    }
}
finally {
    Pop-Location
    Write-Progress -Activity 'Compiling CSB' -Completed
}

if ($Failed.Count -gt 0) {
    Write-Host ""
    Write-Host "$($Failed.Count) file(s) failed to compile:" -ForegroundColor Red
    $Failed | ForEach-Object { Write-Host "  $_" }
    exit 1
}

# ---------------------------------------------------------------- link --

Write-Host "Linking csb.exe ..." -ForegroundColor Cyan
$exe = Join-Path $Build 'csb.exe'
& g++ @($Objects.ToArray()) @LdFlags -o $exe
if ($LASTEXITCODE -ne 0) {
    Write-Host "Link failed." -ForegroundColor Red
    exit 1
}

# The executable needs its DLLs next to it (or on PATH). SDL2 comes from the
# third_party folder; the C++ runtime and pthread come from the compiler, and are
# needed because the toolchain links them dynamically.
$RuntimeDlls = @(
    (Join-Path $SDL 'bin\SDL2.dll'),
    (Join-Path $MinGW 'libstdc++-6.dll'),
    (Join-Path $MinGW 'libgcc_s_seh-1.dll'),
    (Join-Path $MinGW 'libwinpthread-1.dll')
)
foreach ($dll in $RuntimeDlls) {
    if (-not (Test-Path $dll)) {
        throw "Runtime DLL not found: $dll"
    }
    Copy-Item $dll $Build -Force
}

Write-Host ""
Write-Host "Built $exe" -ForegroundColor Green
Get-Item $exe | Select-Object Name, @{n='Size';e={'{0:N0} KB' -f ($_.Length / 1KB)}}, LastWriteTime |
    Format-Table -AutoSize

# ---------------------------------------------------------------- run --

if ($Run) {
    $game = Join-Path $Root 'Dungeons\Dungeonmaster'
    if (-not (Test-Path (Join-Path $game 'dungeon.dat'))) {
        Write-Host "No game data in $game" -ForegroundColor Yellow
        exit 1
    }
    # The engine reads its data files from the working directory.
    Start-Process -FilePath $exe -WorkingDirectory $game
}
