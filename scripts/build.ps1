param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug',
    [string]$ToolchainBin,
    [string]$WorkspaceAlias,
    [switch]$EnableUBSan
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$workspaceRoot = $projectRoot

# Keep both source and dependency-cache spellings ASCII for GCC/Ninja.
if ($projectRoot -match '[^\x00-\x7F]') {
    if (!$WorkspaceAlias) {
        $sha = [System.Security.Cryptography.SHA256]::Create()
        try {
            $bytes = [System.Text.Encoding]::UTF8.GetBytes($projectRoot)
            $id = [System.BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').Substring(0, 12)
        } finally { $sha.Dispose() }
        $WorkspaceAlias = Join-Path ([System.IO.Path]::GetTempPath()) ('MR' + $id)
    }
    $WorkspaceAlias = [System.IO.Path]::GetFullPath($WorkspaceAlias)
    if ($WorkspaceAlias -match '[^\x00-\x7F]') { throw 'WorkspaceAlias must be an ASCII path.' }
    # GCC/libcpp may replace a user-file spelling with a shorter real path.
    # An alias longer than the UTF-8 real root would reintroduce Unicode.
    $aliasBytes = [System.Text.Encoding]::UTF8.GetByteCount($WorkspaceAlias.TrimEnd([char]'\'))
    $sourceBytes = [System.Text.Encoding]::UTF8.GetByteCount($projectRoot.TrimEnd([char]'\'))
    if ($aliasBytes -gt $sourceBytes) {
        throw 'Use a shorter ASCII WorkspaceAlias, or an ASCII physical checkout path.'
    }
    if (Test-Path -LiteralPath $WorkspaceAlias) {
        $item = Get-Item -LiteralPath $WorkspaceAlias -Force
        if ($item.LinkType -ne 'Junction') { throw 'WorkspaceAlias exists and is not a junction.' }
        $target = [System.IO.Path]::GetFullPath(@($item.Target)[0]).TrimEnd([char]'\')
        if (![string]::Equals($target, $projectRoot.TrimEnd([char]'\'), [System.StringComparison]::OrdinalIgnoreCase)) {
            throw 'WorkspaceAlias points to a different directory.'
        }
    } else {
        New-Item -ItemType Junction -Path $WorkspaceAlias -Target $projectRoot | Out-Null
    }
    $workspaceRoot = $WorkspaceAlias
}

$savedBuildPath = $env:PATH
try {
    if ($ToolchainBin) {
        $ToolchainBin = [System.IO.Path]::GetFullPath($ToolchainBin)
        if (!(Test-Path -LiteralPath $ToolchainBin -PathType Container)) { throw 'ToolchainBin does not exist.' }
        if ($ToolchainBin -match '[^\x00-\x7F]') {
            throw 'Use an ASCII toolchain path or an ASCII junction to the complete toolchain directory.'
        }
        $env:PATH = $ToolchainBin + ';' + $savedBuildPath
    }
    $cmake = (Get-Command cmake -ErrorAction Stop).Source
    $ctest = (Get-Command ctest -ErrorAction Stop).Source
    $ninja = (Get-Command ninja -ErrorAction Stop).Source
    $compiler = (Get-Command g++ -ErrorAction Stop).Source
    $buildName = if ($EnableUBSan) { 'UBSan' } else { $Configuration }
    $buildDirectory = Join-Path $workspaceRoot ('build/windows/' + $buildName)
    $sanitizer = if ($EnableUBSan) { 'ON' } else { 'OFF' }
    & $cmake -S $workspaceRoot -B $buildDirectory -G Ninja `
        "-DCMAKE_BUILD_TYPE=$Configuration" "-DCMAKE_CXX_COMPILER=$compiler" `
        "-DCMAKE_MAKE_PROGRAM=$ninja" "-DMINIRASTER_UBSAN=$sanitizer" -DBUILD_TESTING=ON
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $cmake --build $buildDirectory --parallel 2
    if ($LASTEXITCODE -ne 0) { throw 'Compilation failed.' }
    & $ctest --test-dir $buildDirectory --output-on-failure --no-tests=error
    if ($LASTEXITCODE -ne 0) { throw 'CTest failed.' }
} finally {
    $env:PATH = $savedBuildPath
}
