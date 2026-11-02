# Build the standalone LEGO City Undercover port on Windows.
#
# Usage (PowerShell, from the repo folder):
#   powershell -ExecutionPolicy Bypass -File scripts\windows\build-port.ps1 -Rom "C:\path\to\game.cci"
#
# Needs: Git, Rust (rustup) and a C compiler. Two setups work:
#   - GNU (simplest): rustup default stable-x86_64-pc-windows-gnu, plus MinGW-w64 gcc
#     on PATH (e.g. winget install BrechtSanders.WinLibs.POSIX.UCRT).
#   - MSVC (rustup's default): Visual Studio Build Tools, plus LLVM's clang and
#     llvm-ar (winget install LLVM.LLVM); this script sets CC/AR for that case.
#
# Output: work\port\target\release\lego-city-undercover.exe
# Run it with the same ROM: lego-city-undercover.exe "C:\path\to\game.cci"
# The exe contains the game's code compiled: it is for your own copy, don't share it.

param(
    [Parameter(Mandatory = $true)][string]$Rom
)
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Work = Join-Path $Root "work"
New-Item -ItemType Directory -Force -Path $Work | Out-Null

function Need($cmd, $hint) {
    if (-not (Get-Command $cmd -ErrorAction SilentlyContinue)) { throw "$cmd not found. $hint" }
}
function Run($exe, [string[]]$argv) {
    & $exe @argv
    if ($LASTEXITCODE -ne 0) { throw "$exe $($argv -join ' ') failed with exit code $LASTEXITCODE" }
}
Need git "Install Git for Windows."
Need cargo "Install Rust from https://rustup.rs."

# 1. the ROM must be the supported USA revision
$Rom = (Resolve-Path $Rom).Path
$rev = Get-Content (Join-Path $Root "config\supported_revision.json") -Raw | ConvertFrom-Json
Write-Host "Checking ROM hash..."
$hash = (Get-FileHash -Algorithm SHA256 $Rom).Hash.ToLower()
if ($hash -ne $rev.cci.sha256) { throw "ROM hash $hash does not match the supported revision $($rev.cci.sha256)" }

# 2. C compiler: gcc for the GNU toolchain, clang + llvm-ar for MSVC
$host_triple = (& rustc -vV | Select-String "^host:").ToString().Split(" ")[1]
if ($host_triple -like "*-msvc") {
    Need clang "The MSVC Rust toolchain needs LLVM: winget install LLVM.LLVM (or switch to the GNU toolchain)."
    Need llvm-ar "The MSVC Rust toolchain needs LLVM's llvm-ar on PATH."
    $env:CC = "clang"; $env:AR = "llvm-ar"
} else {
    Need gcc "The GNU Rust toolchain needs MinGW-w64 gcc on PATH."
    Need ar "The GNU Rust toolchain needs MinGW-w64 ar on PATH."
}

# 3. 3dsrecomp and Zakuro at the pinned commits, with local patches
$pins = Get-Content (Join-Path $Root "config\upstream.json") -Raw | ConvertFrom-Json
foreach ($name in @("3dsrecomp", "zakuro")) {
    $dir = Join-Path $Work $name
    if (-not (Test-Path (Join-Path $dir ".git"))) {
        Run git @("init", "-q", $dir)
        Run git @("-C", $dir, "remote", "add", "origin", $pins.$name.url)
    }
    Run git @("-C", $dir, "fetch", "-q", "--depth", "1", "origin", $pins.$name.commit)
    Run git @("-C", $dir, "checkout", "-q", "--force", $pins.$name.commit)
    Run git @("-C", $dir, "clean", "-qfd")
    $patchDir = Join-Path $Root "patches\$name"
    if (Test-Path $patchDir) {
        foreach ($p in Get-ChildItem $patchDir -Filter *.patch | Sort-Object Name) {
            Write-Host "applying $($p.Name) to $name"
            Run git @("-C", $dir, "apply", "--whitespace=nowarn", $p.FullName)
        }
    }
}

Push-Location (Join-Path $Work "3dsrecomp")
try { Run cargo @("build", "--release") } finally { Pop-Location }
$recomp = Join-Path $Work "3dsrecomp\target\release\3dsrecomp.exe"

# 4. the places Zakuro had to interpret, so this build recompiles them too
$libDir = Join-Path $env:APPDATA "3dsrecomp"
New-Item -ItemType Directory -Force -Path $libDir | Out-Null
Copy-Item -Force (Join-Path $Root "hints\00040000000AD500.hints") $libDir

# 5. generate the port project and build it
$port = Join-Path $Work "port"
if (Test-Path $port) { Remove-Item -Recurse -Force $port }
Run $recomp @("port", $Rom, $port, "--zakuro", (Join-Path $Work "zakuro"), "--name", "lego-city-undercover", "--overrides", (Join-Path $Root "overrides"))
# build against the same dependency versions Zakuro pins
Copy-Item -Force (Join-Path $Work "zakuro\Cargo.lock") (Join-Path $port "Cargo.lock")
Push-Location $port
try { Run cargo @("build", "--release") } finally { Pop-Location }

$exe = Join-Path $port "target\release\lego-city-undercover.exe"
Write-Host ""
Write-Host "Built: $exe"
Write-Host "Run:   & `"$exe`" `"$Rom`""
