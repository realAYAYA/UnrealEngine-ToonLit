# Copies the supplied list of files from the specified source directory to the System32 directory
function CopyToSystem32($sourceDirectory, $filenames, $rename)
{
	foreach ($filename in $filenames)
	{
		# Determine whether we are renaming the file when we copy it to the destination
		$source = "$sourceDirectory\$filename"
		$destination = "C:\Windows\System32\$filename"
		if ($rename -and $rename[$filename])
		{
			$renamed = $rename[$filename]
			$destination = "C:\Windows\System32\$renamed"
		}
		
		# Perform the copy
		Write-Host "    Copying $source to $destination"
		try {
			Copy-Item -Path "$source" -Destination "$destination" -ErrorAction Stop
		}
		catch {
			Write-Host "    Warning: failed to copy file $filename" -ForegroundColor Yellow
		}
	}
}

# Attempt to locate the NVIDIA Display Driver directory in the host system's driver store
$nvidiaSentinelFile = (Get-ChildItem "C:\Windows\System32\HostDriverStore\FileRepository\nv*.inf_amd64_*\nvapi64.dll" -ErrorAction SilentlyContinue)
if ($nvidiaSentinelFile) {
	
	# Retrieve the path to the directory containing the DLL files for NVIDIA graphics APIs
	$nvidiaDirectory = $nvidiaSentinelFile[0].VersionInfo.FileName | Split-Path
	Write-Host "Found NVIDIA Display Driver directory: $nvidiaDirectory"
	
	# Copy the DLL file for NVAPI to System32
	Write-Host "`nEnabling NVIDIA NVAPI support:"
	CopyToSystem32 `
		-SourceDirectory $nvidiaDirectory `
		-Filenames @("nvapi64.dll")
	
	# Copy the DLL files for NVENC to System32
	Write-Host "`nEnabling NVIDIA NVENC support:"
	CopyToSystem32 `
		-SourceDirectory $nvidiaDirectory `
		-Filenames @("nvEncodeAPI64.dll", "nvEncMFTH264x.dll", "nvEncMFThevcx.dll")
	
	# Copy the DLL files for NVDEC (formerly known as CUVID) to System32
	Write-Host "`nEnabling NVIDIA CUVID/NVDEC support:"
	CopyToSystem32 `
		-SourceDirectory $nvidiaDirectory `
		-Filenames @("nvcuvid64.dll", "nvDecMFTMjpeg.dll", "nvDecMFTMjpegx.dll") `
		-Rename @{"nvcuvid64.dll" = "nvcuvid.dll"}
	
	# Copy the DLL files for CUDA to System32
	Write-Host "`nEnabling NVIDIA CUDA support:"
	CopyToSystem32 `
		-SourceDirectory $nvidiaDirectory `
		-Filenames @("nvcuda64.dll", "nvcuda_loader64.dll", "nvptxJitCompiler64.dll") `
		-Rename @{"nvcuda_loader64.dll" = "nvcuda.dll"}
	
	# Print a blank line before any subsequent output
	Write-Host ""
}

# Attempt to locate the AMD Display Driver directory in the host system's driver store
$amdSentinelFile = (Get-ChildItem "C:\Windows\System32\HostDriverStore\FileRepository\u*.inf_amd64_*\*\aticfx64.dll" -ErrorAction SilentlyContinue)
if ($amdSentinelFile) {
	
	# Retrieve the path to the directory containing the DLL files for AMD graphics APIs
	$amdDirectory = $amdSentinelFile[0].VersionInfo.FileName | Split-Path
	Write-Host "Found AMD Display Driver directory: $amdDirectory"
	
	# Copy the DLL files for the AMD DirectX drivers to System32
	# (Note that copying these files to System32 is not necessary for rendering, but applications using ADL may attempt to load these files from System32)
	Write-Host "`nCopying AMD DirectX driver files:"
	CopyToSystem32 `
		-SourceDirectory $amdDirectory `
		-Filenames @(`
			"aticfx64.dll", `
			"atidxx64.dll"
		)

	# Copy the DLL files needed for AMD Display Library (ADL) support to System32
	Write-Host "`nEnabling AMD Display Library (ADL) support:"
	CopyToSystem32 `
		-SourceDirectory $amdDirectory `
		-Filenames @(`
			"atiadlxx.dll", `
			"atiadlxy.dll" `
		)
	
	# Copy the DLL files needed for AMD Advanced Media Framework (AMF) support to System32
	Write-Host "`nEnabling AMD Advanced Media Framework (AMF) support:"
	CopyToSystem32 `
		-SourceDirectory $amdDirectory `
		-Filenames @(`
			"amfrt64.dll", `
			"amfrtdrv64.dll", `
			"amdihk64.dll" `
		)
	
	# Print a blank line before any subsequent output
	Write-Host ""
}
