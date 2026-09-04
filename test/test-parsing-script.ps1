$routerUser = "admin"
$routerPassword = "nXfR8MexyxeCozJBopLE"
$routerUrl = "http://192.168.1.1/RST_stattbl.htm"

$html = curl.exe -s `
    -u "${routerUser}:${routerPassword}" `
    $routerUrl

$wanMarker = '<span class="thead">WAN</span>'

$wanPosition = $html.IndexOf($wanMarker)

if ($wanPosition -eq -1) {
    Write-Host "WAN marker not found"
    exit
}

$length = [Math]::Min(400, $html.Length - $wanPosition)

$wanSection = $html.Substring($wanPosition, $length)

Write-Host "Found WAN section:"
Write-Host "------------------"
Write-Host $wanSection
Write-Host "------------------"

if ($wanSection.Contains("1000M/Full")) {
    Write-Host "PARSED RESULT: 1000 Mbps"
}
elseif ($wanSection.Contains("100M/Full")) {
    Write-Host "PARSED RESULT: 100 Mbps"
}
else {
    Write-Host "PARSED RESULT: UNKNOWN"
}