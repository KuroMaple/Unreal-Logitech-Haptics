# Exercises the MXBridge HTTP endpoints using the token file the editor writes.
# Usage:
#   .\scripts\test-bridge.ps1 -Project "C:\Users\you\Documents\Unreal Projects\GodTree"
#   .\scripts\test-bridge.ps1 -Project ... -Command pie.play
param(
    [Parameter(Mandatory = $true)][string]$Project,
    [string]$Command
)

$tokenPath = Join-Path $Project "Saved\MXBridge\bridge.token"
if (-not (Test-Path $tokenPath)) {
    Write-Error "No token file at $tokenPath - is the editor running with MXBridge enabled?"
    exit 1
}

$info = Get-Content $tokenPath -Raw | ConvertFrom-Json
$base = "http://127.0.0.1:$($info.port)"
$headers = @{ "X-MXBridge-Token" = $info.token }
Write-Host "Editor pid $($info.pid), port $($info.port)"

Write-Host "`nGET /context"
Invoke-RestMethod -Uri "$base/context" -Headers $headers | ConvertTo-Json -Depth 6

Write-Host "`nGET /context without token (expect 401)"
try { Invoke-RestMethod -Uri "$base/context" | Out-Null; Write-Host "UNEXPECTED: succeeded" }
catch { Write-Host "-> $($_.Exception.Response.StatusCode.value__)" }

if ($Command) {
    Write-Host "`nPOST /events $Command"
    $body = @{ schema = 1; events = @(@{ id = $Command; kind = "trigger" }) } | ConvertTo-Json -Depth 4
    Invoke-RestMethod -Uri "$base/events" -Method Post -Headers $headers -ContentType "application/json" -Body $body |
        ConvertTo-Json
}
