# SPDX-License-Identifier: LGPL-3.0-or-later
# Copyright (C) 2026 Cavanshirpro
# Licensed under LGPL-3.0-or-later; see NOTICE, LICENSE and COPYING.
param([string]$Msys2Root = 'C:/msys64', [switch]$Shared)
$ErrorActionPreference = 'Stop'
$taskBin = Join-Path $Msys2Root 'mingw64/bin'
if (-not (Test-Path -LiteralPath (Join-Path $taskBin 'g++.exe'))) {
    throw "MINGW64 compiler not found under $taskBin"
}
$env:PATH = "$taskBin;$env:PATH"
$taskRoot = Split-Path -Parent $PSScriptRoot
$taskBuild = Join-Path $taskRoot 'build/powershell'
$taskShared = if ($Shared) { 'ON' } else { 'OFF' }
cmake -S $taskRoot -B $taskBuild -G Ninja "-DCMAKE_CXX_COMPILER=$taskBin/g++.exe" "-DBUILD_SHARED_LIBS=$taskShared" -DCMAKE_BUILD_TYPE=Debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $taskBuild --parallel 2
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $taskBuild --output-on-failure
exit $LASTEXITCODE
