Please see https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist for latest installer.

Update version numbers to match in:
Engine\Source\Programs\Windows\BootstrapPackagedGame\Private\BootstrapPackagedGame.cpp

Copy redist files from:
C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Redist\MSVC\<version>\x86\Microsoft.VC143.CRT
to:
Engine\Binaries\ThirdParty\AppLocalDependencies\Win64\Microsoft.VC.CRT