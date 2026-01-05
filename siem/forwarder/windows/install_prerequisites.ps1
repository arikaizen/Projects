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

# Check for Visual Studio
Write-Host "[3/4] Checking for Visual Studio..." -ForegroundColor White
$vsFound = $false

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
        $vsFound = $true
        break
    }
}

# Check for MinGW as alternative
if (-not $vsFound) {
    try {
        $gccVersion = g++ --version 2>$null
        if ($gccVersion) {
            Write-Host "  [OK] MinGW-w64 found (alternative to Visual Studio)" -ForegroundColor Green
            $vsFound = $true
        }
    } catch {
        Write-Host "  [MISSING] Visual Studio or MinGW not found" -ForegroundColor Red
        $missingTools += "Visual Studio"
        $adminRequired = $true
    }
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

# Install .NET Framework
if ($missingTools -contains ".NET Framework") {
    Write-Host "Missing: .NET Framework 4.0 or higher" -ForegroundColor Red
    Write-Host "  Required for: Visual Studio installation" -ForegroundColor Yellow
    Write-Host ""

    $installDotNet = Read-Host "Would you like to download and install .NET Framework 4.8? (Y/N)"

    if ($installDotNet -eq "Y" -or $installDotNet -eq "y") {
        Write-Host ""
        Write-Host "Installing .NET Framework 4.8..." -ForegroundColor Yellow

        # .NET Framework 4.8 Runtime installer URL
        $dotnetUrl = "https://go.microsoft.com/fwlink/?linkid=2088631"
        $dotnetInstaller = "$env:TEMP\ndp48-web.exe"

        try {
            Write-Host "  Downloading .NET Framework 4.8 installer..." -ForegroundColor Yellow
            Download-FileWithProgress -Url $dotnetUrl -Destination $dotnetInstaller

            Write-Host "  Running .NET Framework installer..." -ForegroundColor Yellow
            Write-Host "  This may take several minutes..." -ForegroundColor Cyan
            Write-Host "  A system restart will be required after installation" -ForegroundColor Yellow

            Start-Process -FilePath $dotnetInstaller -ArgumentList "/q /norestart" -Wait

            Write-Host "  .NET Framework installation complete!" -ForegroundColor Green
            Write-Host "  IMPORTANT: You must restart your computer before installing Visual Studio" -ForegroundColor Red

            # Clean up installer
            Remove-Item $dotnetInstaller -ErrorAction SilentlyContinue

            $restart = Read-Host "Restart computer now? (Y/N)"
            if ($restart -eq "Y" -or $restart -eq "y") {
                Write-Host "  Restarting computer..." -ForegroundColor Yellow
                Restart-Computer -Force
            }
        } catch {
            Write-Host "  [ERROR] Failed to install .NET Framework automatically" -ForegroundColor Red
            Write-Host "  Please download manually from: https://dotnet.microsoft.com/download/dotnet-framework/net48" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  Manual installation required:" -ForegroundColor Yellow
        Write-Host "  Download from: https://go.microsoft.com/fwlink/?linkid=2088631" -ForegroundColor White
        Write-Host "  After installation, restart your computer" -ForegroundColor White
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

# Handle Visual Studio
if ($missingTools -contains "Visual Studio") {
    Write-Host ""
    Write-Host "Missing: Visual Studio or MinGW" -ForegroundColor Red
    Write-Host ""
    Write-Host "Visual Studio requires manual installation (large download ~5GB)" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "Option 1 - Visual Studio Community 2022 (Recommended):" -ForegroundColor Cyan
    Write-Host "  Download: https://visualstudio.microsoft.com/downloads/" -ForegroundColor White
    Write-Host "  Required workload: 'Desktop development with C++'" -ForegroundColor White
    Write-Host ""
    Write-Host "Option 2 - MinGW-w64 (Lightweight, ~500MB):" -ForegroundColor Cyan
    Write-Host "  Download MSYS2: https://www.msys2.org/" -ForegroundColor White
    Write-Host "  After installing MSYS2, run: pacman -S mingw-w64-x86_64-toolchain" -ForegroundColor White
    Write-Host ""

    $openVS = Read-Host "Open Visual Studio download page in browser? (Y/N)"
    if ($openVS -eq "Y" -or $openVS -eq "y") {
        Start-Process "https://visualstudio.microsoft.com/downloads/"
        Write-Host "  Browser opened to Visual Studio download page" -ForegroundColor Green
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
