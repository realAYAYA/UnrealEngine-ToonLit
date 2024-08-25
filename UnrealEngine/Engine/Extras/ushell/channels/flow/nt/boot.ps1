Write-Debug "Ushell bootstrap args 2: $Args"

$Working = Join-Path $env:LOCALAPPDATA "ushell" ".working"
if(![String]::IsNullOrEmpty($env:flow_working_dir)) {
    $Working = $env:flow_working_dir
}

Write-Debug "Working dir: $Working"

$Channels = @(Join-Path $PSScriptRoot "..\..\..\channels")
$Channels += Join-Path $env:userprofile "\.ushell\channels"
$Channels += $env:flow_channels_dir

Write-Debug "Channels: $Channels"

$ProvisionBat = Join-Path $PSScriptRoot "provision.bat"
Write-Debug "Provisioning: $ProvisionBat"
& "cmd.exe" "/d/c" "`"$ProvisionBat`" $Working"

if(!$?) {
    throw "Ushell provisioning failed"
}


$PythonPath = Join-Path $Working "python\current\flow_python.exe"
$Bootpy = Join-Path $PSScriptRoot "..\core\system\boot.py"
Write-Debug "Python: $PythonPath"

$TempDir = Join-Path Temp: "ushell"
New-Item -ItemType Directory -Force $TempDir
$Cookie = Join-Path $TempDir "cmd_boot_$(New-Guid).ps1"
$Cookie = New-Item $Cookie
Write-Debug "Cookie: $Cookie"

& $PythonPath "-Xutf8" "-Esu" $Bootpy $Working @Channels "--" "--bootarg=pwsh,$Cookie" $Args | Out-Host
$PythonReturn = $?

if(!$PythonReturn) {
    throw "Python boot failed"
}

try {
    Import-Module $Cookie
}
finally {
    if( Test-Path $Cookie )
    {
        Write-Debug "Removing cookie: $Cookie"
        Remove-Item $Cookie
    }
}