# Requires powershell 7
[CmdletBinding(SupportsShouldProcess=$True)]
param (
)

# Copy headers 
Copy-Item (Join-Path $PSScriptRoot "git/src/*.h") (Join-Path $PSScriptRoot "include/raw_pdb") -Verbose:$VerbosePreference -WhatIf:$WhatIfPreference
Copy-Item (Join-Path $PSScriptRoot "git/src/Foundation/*.h") (Join-Path $PSScriptRoot "include/raw_pdb/Foundation") -Verbose:$VerbosePreference -WhatIf:$WhatIfPreference

# Copy binaries
Copy-Item (Join-Path $PSScriptRoot "git/bin/x64/Release/*") (Join-Path $PSScriptRoot "bin/Win64")
Copy-Item (Join-Path $PSScriptRoot "git/lib/x64/Release/*") (Join-Path $PSScriptRoot "bin/Win64")