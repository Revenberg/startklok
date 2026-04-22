param(
    [string]$IP = "192.168.1.200",
    [switch]$Watch
)

function Test-ESP32 {
    param([string]$IPAddress)
    
    Write-Host "`n=== Testing ESP32 at $IPAddress ===" -ForegroundColor Cyan
    
    # Test version
    Write-Host "`n[1/4] Version:" -ForegroundColor Yellow
    try {
        $version = curl -s "http://$IPAddress/version" 2>$null
        if ($version) {
            Write-Host $version -ForegroundColor Green
        } else {
            Write-Host "No response" -ForegroundColor Red
        }
    } catch {
        Write-Host "Connection failed" -ForegroundColor Red
    }
    
    # Test status API
    Write-Host "`n[2/4] Status API:" -ForegroundColor Yellow
    try {
        $status = curl -s "http://$IPAddress/status" 2>$null
        if ($status) {
            Write-Host $status -ForegroundColor Green
        } else {
            Write-Host "No response" -ForegroundColor Red
        }
    } catch {
        Write-Host "Connection failed" -ForegroundColor Red
    }
    
    # Test dashboard
    Write-Host "`n[3/4] Dashboard:" -ForegroundColor Yellow
    try {
        $response = Invoke-WebRequest -Uri "http://$IPAddress/" -TimeoutSec 5 -ErrorAction Stop
        if ($response.StatusCode -eq 200) {
            Write-Host "✓ Available (HTTP 200)" -ForegroundColor Green
        }
    } catch {
        Write-Host "✗ Not available" -ForegroundColor Red
    }
    
    # Test setup page
    Write-Host "`n[4/4] Setup page:" -ForegroundColor Yellow
    try {
        $response = Invoke-WebRequest -Uri "http://$IPAddress/setup" -TimeoutSec 5 -ErrorAction Stop
        if ($response.StatusCode -eq 200) {
            Write-Host "✓ Available (HTTP 200)" -ForegroundColor Green
        }
    } catch {
        Write-Host "✗ Not available" -ForegroundColor Red
    }
    
    # Show URLs
    Write-Host "`n=== Quick Access URLs ===" -ForegroundColor Cyan
    Write-Host "Dashboard:  http://$IPAddress/" -ForegroundColor White
    Write-Host "Setup:      http://$IPAddress/setup" -ForegroundColor White
    Write-Host "Status API: http://$IPAddress/status" -ForegroundColor White
    Write-Host "Version:    http://$IPAddress/version" -ForegroundColor White
    Write-Host "WebSocket:  ws://$IPAddress:81" -ForegroundColor White
}

if ($Watch) {
    Write-Host "Watching mode - press Ctrl+C to exit" -ForegroundColor Cyan
    while ($true) {
        Clear-Host
        Test-ESP32 -IPAddress $IP
        Write-Host "`nRefreshing in 5 seconds..." -ForegroundColor Gray
        Start-Sleep -Seconds 5
    }
} else {
    Test-ESP32 -IPAddress $IP
}
