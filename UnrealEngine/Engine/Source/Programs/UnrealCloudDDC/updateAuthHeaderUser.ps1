$TempFile = New-TemporaryFile

& "dotnet" "..\..\..\..\Engine\Binaries\DotNET\OidcToken\portable\OidcToken.dll" "--Service=EpicGames-Okta" "--OutFile" $TempFile.FullName 
$env:curl_auth_header = Get-Content -Raw -Path $TempFile.FullName | ConvertFrom-Json | ForEach-Object { "Authorization: Bearer $($_.Token)" }
$env:auth_token = Get-Content -Raw -Path $TempFile.FullName | ConvertFrom-Json | ForEach-Object { "$($_.Token)" }
