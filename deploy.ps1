param(
    [string]$Port = "",
    [switch]$SkipGit,
    [switch]$Verbose,
    [switch]$NoVersionBump
)

Write-Host "=== ESP32 Deploy Workflow ===" -ForegroundColor Cyan

# 0. Increment version
if (-not $NoVersionBump) {
    Write-Host "`n[0/5] Incrementing version..." -ForegroundColor Yellow
    
    $versionFile = "version.h"
    if (Test-Path $versionFile) {
        $content = Get-Content $versionFile -Raw
        
        # Extract current build number
        if ($content -match '#define BUILD_NUMBER (\d+)') {
            $currentBuild = [int]$matches[1]
            $newBuild = $currentBuild + 1
            
            # Extract version numbers
            if ($content -match '#define VERSION_MAJOR (\d+)') { $major = $matches[1] }
            if ($content -match '#define VERSION_MINOR (\d+)') { $minor = $matches[1] }
            if ($content -match '#define VERSION_PATCH (\d+)') { $patch = $matches[1] }
            
            # Update build number
            $content = $content -replace '#define BUILD_NUMBER \d+', "#define BUILD_NUMBER $newBuild"
            
            # Update version string
            $newVersion = "v$major.$minor.$patch-b$newBuild"
            $content = $content -replace '#define VERSION_STRING ".*"', "#define VERSION_STRING `"$newVersion`""
            
            Set-Content -Path $versionFile -Value $content
            
            Write-Host "Version bumped: $newVersion" -ForegroundColor Green
        }
    } else {
        Write-Host "version.h not found, skipping version bump" -ForegroundColor Yellow
    }
}

# 1. Git
if (-not $SkipGit) {
    Write-Host "`n[1/6] Checking Git..." -ForegroundColor Yellow
    git status
    $gitChanges = git status --porcelain
    if ($gitChanges) {
        $push = Read-Host "Push changes to GitHub? (y/n)"
        if ($push -eq 'y') {
            git add .
            $msg = Read-Host "Commit message"
            git commit -m $msg
            git push
        }
    } else {
        Write-Host "No changes to commit" -ForegroundColor Green
    }
}

# 2. Clean
Write-Host "`n[2/6] Cleaning build cache..." -ForegroundColor Yellow
Remove-Item -Path "$env:LOCALAPPDATA\arduino\sketches\*" -Recurse -Force -ErrorAction SilentlyContinue
Write-Host "Build cache cleared" -ForegroundColor Green

# 3. Compile
Write-Host "`n[3/6] Compiling..." -ForegroundColor Yellow
$compileCmd = "arduino-cli compile --fqbn esp32:esp32:esp32 --output-dir build ."
if ($Verbose) { $compileCmd += " --verbose" }
Invoke-Expression $compileCmd

if ($LASTEXITCODE -ne 0) {
    Write-Host "`nCompile failed!" -ForegroundColor Red
    exit 1
}
Write-Host "Compile successful!" -ForegroundColor Green

# 4. Detect boards
Write-Host "`n[4/6] Detecting boards..." -ForegroundColor Yellow
$boardList = arduino-cli board list | Out-String
Write-Host $boardList

# Auto-detect ESP32 port if not specified
if ([string]::IsNullOrEmpty($Port)) {
    $espBoards = arduino-cli board list | Select-String "COM\d+" -AllMatches | ForEach-Object { $_.Matches.Value } | Select-Object -Unique
    
    if ($espBoards.Count -eq 1) {
        $Port = $espBoards[0]
        Write-Host "Auto-detected ESP32 on port: $Port" -ForegroundColor Cyan
    } elseif ($espBoards.Count -gt 1) {
        Write-Host "Multiple boards detected:" -ForegroundColor Yellow
        $espBoards | ForEach-Object { Write-Host "  - $_" }
        $Port = Read-Host "Select COM port (e.g., COM7)"
    } else {
        Write-Host "No ESP32 board detected!" -ForegroundColor Red
        $Port = Read-Host "Enter COM port manually (e.g., COM7)"
    }
}

# 5. Upload
Write-Host "`n[5/6] Uploading to $Port..." -ForegroundColor Yellow
$uploadCmd = "arduino-cli upload -p $Port --fqbn esp32:esp32:esp32 ."
if ($Verbose) { $uploadCmd += " --verbose" }
Invoke-Expression $uploadCmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "`n=== Deploy Successful! ===" -ForegroundColor Green
    Write-Host "Board is ready on $Port" -ForegroundColor Cyan
    
    # 6. Commit version bump
    if (-not $SkipGit -and -not $NoVersionBump) {
        Write-Host "`n[6/6] Committing version bump..." -ForegroundColor Yellow
        $versionContent = Get-Content "version.h" -Raw
        if ($versionContent -match '#define VERSION_STRING "(.*)"') {
            $version = $matches[1]
            git add version.h data/index.html
            git commit -m "Bump version to $version" -q
            Write-Host "Version $version committed locally" -ForegroundColor Green
            Write-Host "(Use 'git push' to push to GitHub)" -ForegroundColor Cyan
        }
    }
    
    $monitor = Read-Host "`nStart serial monitor? (y/n)"
    if ($monitor -eq 'y') {
        Write-Host "Starting serial monitor (Ctrl+C to exit)..." -ForegroundColor Yellow
        arduino-cli monitor -p $Port -c baudrate=115200
    }
} else {
    Write-Host "`n=== Deploy Failed! ===" -ForegroundColor Red
    exit 1
}
