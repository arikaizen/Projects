# Windows Event Log Forwarder - Prerequisite Auto-Installer
# This PowerShell script checks for and installs missing build tools

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Windows Event Log Forwarder" -ForegroundColor Cyan
Write-Host "Automated Prerequisite Installer" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

$missingTools = @()
$adminRequired = $false

# Function to check if running as administrator
function Test-Administrator {
    $currentUser = New-Object Security.Principal.WindowsPrincipal([Security.Principal.WindowsIdentity]::GetCurrent())
    return $currentUser.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Function to download file with progress
function Download-FileWithProgress {
    param(
        [string]$Url,
        [string]$Destination
    )

    Write-Host "  Downloading from: $Url" -ForegroundColor Yellow

    $webClient = New-Object System.Net.WebClient
    $webClient.DownloadFile($Url, $Destination)

    Write-Host "  Download complete: $Destination" -ForegroundColor Green
}

# Check for .NET Framework
Write-Host "[1/4] Checking for .NET Framework..." -ForegroundColor White
$dotnetInstalled = $false
try {
    $dotnetVersion = Get-ItemProperty -Path "HKLM:\SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full" -ErrorAction Stop
    if ($dotnetVersion.Version) {
        Write-Host "  [OK] .NET Framework found: $($dotnetVersion.Version)" -ForegroundColor Green
        $dotnetInstalled = $true
    }
} catch {
    Write-Host "  [MISSING] .NET Framework 4.0 or higher not found" -ForegroundColor Red
    Write-Host "  [INFO] Required for installing Visual Studio" -ForegroundColor Yellow
    $missingTools += ".NET Framework"
}

# Check for CMake
Write-Host "[2/4] Checking for CMake..." -ForegroundColor White
$cmakeInstalled = $false
try {
    $cmakeVersion = cmake --version 2>$null
    if ($cmakeVersion) {
        Write-Host "  [OK] CMake found: $($cmakeVersion[0])" -ForegroundColor Green
        $cmakeInstalled = $true
    }
} catch {
    Write-Host "  [MISSING] CMake not found" -ForegroundColor Red
    $missingTools += "CMake"
}

# Check for C++ Compiler (MinGW or Visual Studio)
Write-Host "[3/4] Checking for C++ Compiler..." -ForegroundColor White
$compilerFound = $false

# Check for MinGW (recommended)
try {
    $gccVersion = g++ --version 2>$null
    if ($gccVersion) {
        Write-Host "  [OK] MinGW g++ found: $($gccVersion[0])" -ForegroundColor Green
        $compilerFound = $true
    }
} catch {
    # MinGW not found, continue checking
}

# Check for Visual Studio as alternative
if (-not $compilerFound) {
    $vsPaths = @(
        "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat",
        "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvarsall.bat"
    )

    foreach ($path in $vsPaths) {
        if (Test-Path $path) {
            $vsVersion = if ($path -like "*2022*") { "2022" } else { "2019" }
            $vsEdition = if ($path -like "*Community*") { "Community" } elseif ($path -like "*Professional*") { "Professional" } else { "Enterprise" }
            Write-Host "  [OK] Visual Studio $vsVersion $vsEdition found" -ForegroundColor Green
            $compilerFound = $true
            break
        }
    }
}

if (-not $compilerFound) {
    Write-Host "  [MISSING] C++ Compiler not found" -ForegroundColor Red
    $missingTools += "Compiler"
}

# Check for Windows SDK
Write-Host "[4/4] Checking for Windows SDK..." -ForegroundColor White
if (Test-Path "C:\Program Files (x86)\Windows Kits\10\Include") {
    Write-Host "  [OK] Windows SDK 10 found" -ForegroundColor Green
} elseif (Test-Path "C:\Program Files (x86)\Windows Kits\8.1\Include") {
    Write-Host "  [OK] Windows SDK 8.1 found" -ForegroundColor Green
} else {
    Write-Host "  [INFO] Windows SDK not detected (usually included with Visual Studio)" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Prerequisite Check Complete" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# If all tools are installed
if ($missingTools.Count -eq 0) {
    Write-Host "[SUCCESS] All prerequisites are installed!" -ForegroundColor Green
    Write-Host ""
    Write-Host "You can now build the forwarder by running:" -ForegroundColor White
    Write-Host "  build.bat" -ForegroundColor Yellow
    Write-Host ""
    pause
    exit 0
}

# Handle missing tools
Write-Host "[WARNING] Missing prerequisites detected" -ForegroundColor Yellow
Write-Host ""

# Note: .NET Framework only needed for Visual Studio (not MinGW)
if ($missingTools -contains ".NET Framework") {
    if ($missingTools -contains "Compiler") {
        Write-Host "[INFO] .NET Framework 4.0 or higher not found" -ForegroundColor Yellow
        Write-Host "  Only required if you choose to install Visual Studio" -ForegroundColor White
        Write-Host "  Not needed for MinGW (recommended)" -ForegroundColor White
        Write-Host ""
    }
}

# Install CMake automatically
if ($missingTools -contains "CMake") {
    Write-Host "Missing: CMake" -ForegroundColor Red
    Write-Host ""

    $installCMake = Read-Host "Would you like to download and install CMake automatically? (Y/N)"

    if ($installCMake -eq "Y" -or $installCMake -eq "y") {
        Write-Host ""
        Write-Host "Installing CMake..." -ForegroundColor Yellow

        # CMake installer URL (latest stable)
        $cmakeUrl = "https://github.com/Kitware/CMake/releases/download/v3.28.1/cmake-3.28.1-windows-x86_64.msi"
        $cmakeInstaller = "$env:TEMP\cmake-installer.msi"

        try {
            Write-Host "  Downloading CMake installer..." -ForegroundColor Yellow
            Download-FileWithProgress -Url $cmakeUrl -Destination $cmakeInstaller

            Write-Host "  Running CMake installer..." -ForegroundColor Yellow
            Write-Host "  (Make sure to check 'Add CMake to system PATH' during installation)" -ForegroundColor Cyan

            Start-Process msiexec.exe -ArgumentList "/i `"$cmakeInstaller`" /qb" -Wait

            Write-Host "  CMake installation complete!" -ForegroundColor Green
            Write-Host "  Please close and reopen your terminal for PATH changes to take effect" -ForegroundColor Yellow

            # Clean up installer
            Remove-Item $cmakeInstaller -ErrorAction SilentlyContinue
        } catch {
            Write-Host "  [ERROR] Failed to install CMake automatically" -ForegroundColor Red
            Write-Host "  Please download manually from: https://cmake.org/download/" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  Manual installation required:" -ForegroundColor Yellow
        Write-Host "  Download from: https://cmake.org/download/" -ForegroundColor White
        Write-Host "  Recommended: cmake-3.28.1-windows-x86_64.msi" -ForegroundColor White
        Write-Host ""
    }
}

# Handle C++ Compiler installation
if ($missingTools -contains "Compiler") {
    Write-Host ""
    Write-Host "Missing: C++ Compiler" -ForegroundColor Red
    Write-Host ""
    Write-Host "Option 1 - MinGW-w64 (Recommended - Lightweight ~500MB):" -ForegroundColor Cyan
    Write-Host "  Manual installation:" -ForegroundColor White
    Write-Host "    1. Download from: https://github.com/niXman/mingw-builds-binaries/releases" -ForegroundColor White
    Write-Host "    2. Get file: x86_64-*-posix-seh-ucrt-*.7z" -ForegroundColor White
    Write-Host "    3. Extract to C:\mingw64" -ForegroundColor White
    Write-Host "    4. Add to PATH: C:\mingw64\bin" -ForegroundColor White
    Write-Host ""
    Write-Host "  Alternative - MSYS2 (includes package manager):" -ForegroundColor White
    Write-Host "    Download: https://www.msys2.org/" -ForegroundColor White
    Write-Host "    After install: pacman -S mingw-w64-x86_64-gcc" -ForegroundColor White
    Write-Host ""
    Write-Host "Option 2 - Visual Studio (Alternative - Large ~7GB):" -ForegroundColor Cyan
    Write-Host "  Requires: .NET Framework 4.8+ (not installed)" -ForegroundColor Yellow
    Write-Host "  Download: https://visualstudio.microsoft.com/downloads/" -ForegroundColor White
    Write-Host "  Install: 'Desktop development with C++' workload" -ForegroundColor White
    Write-Host ""

    $choice = Read-Host "Choose: [1] Open MinGW download, [2] Open Visual Studio download, [N] Skip"

    if ($choice -eq "1") {
        Start-Process "https://github.com/niXman/mingw-builds-binaries/releases"
        Write-Host "  Opening MinGW download page..." -ForegroundColor Green
        Write-Host "  After extracting, add C:\mingw64\bin to your PATH" -ForegroundColor Yellow
    } elseif ($choice -eq "2") {
        Start-Process "https://visualstudio.microsoft.com/downloads/"
        Write-Host "  Opening Visual Studio download page..." -ForegroundColor Green
        Write-Host "  Install 'Desktop development with C++' workload" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Next Steps" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Complete any manual installations above" -ForegroundColor White
Write-Host "2. Restart your terminal/PowerShell" -ForegroundColor White
Write-Host "3. Run this script again to verify: .\install_prerequisites.ps1" -ForegroundColor Yellow
Write-Host "4. Build the forwarder: .\build.bat" -ForegroundColor Yellow
Write-Host ""
pause
