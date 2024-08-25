# Our supported command-line parameters
param (
	[string]$release = ""
)

# Determine if a specific Windows release was specified or if we are auto-detecting the host OS release
if ($release -ne "")
{
	# Use Hyper-V isolation mode to ensure compatibility with the specified Windows release
	$windowsRelease = $release
	$isolation = "hyperv"
	
	# Append the release to the image tag
	$tag = "runtime-windows-$release"
}
else
{
	# Determine whether we are running under Windows Server 2022 / Windows 11, or an older version of Windows
	[int]$kernelBuild = (Get-ItemProperty -Path 'Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -Name CurrentBuildNumber).CurrentBuildNumber
	if ($kernelBuild -ge 20348)
	{
		# Use a Windows Server 2022 base image
		$windowsRelease = "ltsc2022"
		
		# Use process isolation mode for improved performance
		$isolation = "process"
	}
	else
	{
		# Use a Windows Server 2019 base image
		$windowsRelease = "ltsc2019"
		
		# Use process isolation mode if we're running under Windows Server 2019 itself, otherwise use Hyper-V isolation mode (e.g. when running under Windows 10)
		if ($kernelBuild -eq 17763) {
			$isolation = "process"
		} else {
			$isolation = "hyperv"
		}
	}
	
	# Don't suffix the image tag
	$tag = "runtime-windows"
}

# Determine the appropriate image to copy DLL files from
$dllImage = "mcr.microsoft.com/windows"
if ($windowsRelease -eq "ltsc2022") {
	$dllImage = "mcr.microsoft.com/windows/server"
}

# Identify any DLL files that are required for the specific version of Windows
$versionSpecificDLLs = ""
if ($windowsRelease -eq "ltsc2019") {
	$versionSpecificDLLs = "ksuser.dll"
}

# Build our runtime container image using the correct base image for the selected Windows version
"Building runtime container image for Windows version $windowsRelease with ``$isolation`` isolation mode and DLL files from ``$dllImage``..."
docker build -t "ghcr.io/epicgames/unreal-engine:$tag" --isolation="$isolation" --build-arg "DLL_IMAGE=$dllImage" --build-arg "BASETAG=$windowsRelease" --build-arg "VERSION_SPECIFIC_DLLS=$versionSpecificDLLs" ./runtime
