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
	# Retrieve the Windows release number (e.g. 1903, 1909, 2004, 20H2, etc.)
	$displayVersion = (Get-ItemProperty -Path 'Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -Name DisplayVersion -ErrorAction SilentlyContinue)
	if ($displayVersion) {
		$windowsRelease = $displayVersion.DisplayVersion
	} else {
		$windowsRelease = (Get-ItemProperty -Path 'Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -Name ReleaseId).ReleaseId
	}
	
	# Use LTSC2022 images under Windows 11 and Windows Server 2022
	[int]$kernelBuild = (Get-ItemProperty -Path 'Registry::HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows NT\CurrentVersion' -Name CurrentBuildNumber).CurrentBuildNumber
	if ($kernelBuild -ge 20348) {
		$windowsRelease = "ltsc2022"
	}

	# Use process isolation mode for improved performance
	$isolation = "process"
	
	# Don't suffix the image tag
	$tag = "runtime-windows"
}

# Determine the appropriate image to copy DLL files from
$dllImage = "mcr.microsoft.com/windows"
if ($windowsRelease -eq "ltsc2022") {
	$dllImage = "mcr.microsoft.com/windows/server"
}

# Build our runtime container image using the correct base image for the selected Windows version
"Building runtime container image for Windows version $windowsRelease with ``$isolation`` isolation mode and DLL files from ``$dllImage``..."
docker build -t "ghcr.io/epicgames/unreal-engine:$tag" --isolation="$isolation" --build-arg "DLL_IMAGE=$dllImage" --build-arg "BASETAG=$windowsRelease" ./runtime
