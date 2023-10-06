// Copyright Epic Games, Inc. All Rights Reserved.
using System.IO;
using UnrealBuildTool;
using System;

public class RivermaxLib : ModuleRules
{
	public RivermaxLib(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			string RivermaxDir = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "Mellanox", "Rivermax");
			string MellanoxInstallPath = Microsoft.Win32.Registry.GetValue(@"HKEY_LOCAL_MACHINE\SOFTWARE\Mellanox\MLNX_WinOF2", "InstalledPath", null) as string;

			if (MellanoxInstallPath != null)
			{
				DirectoryInfo ParentDir = Directory.GetParent(MellanoxInstallPath);
				RivermaxDir = Path.Combine(ParentDir.Parent.FullName, "Rivermax"); 
			}
			
			string RivermaxLibDir = Path.Combine(RivermaxDir, "Lib");
			
			//This is required because Rivermax depends on other drivers / dll to be installed for mellanox. We will manually load the dll and gracefully fail instead of 
			//failing to load the module entirely.
			PublicDelayLoadDLLs.Add("rivermax.dll");
			
			PublicRuntimeLibraryPaths.Add(RivermaxLibDir);
			
			//Used during manual loading of the library
			PublicDefinitions.Add("RIVERMAX_LIBRARY_PLATFORM_PATH=" + RivermaxLibDir.Replace(@"\", "/"));
			PublicDefinitions.Add("RIVERMAX_LIBRARY_NAME=" + "rivermax.dll");
		
			string SDKThirdPartyPath = Path.Combine(Target.UEThirdPartySourceDirectory, "NVIDIA/Rivermax");
			PublicSystemIncludePaths.Add(Path.Combine(SDKThirdPartyPath,"include"));
		}
	}
}

