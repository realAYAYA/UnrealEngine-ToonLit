// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Globalization;
using System.IO;
using UnrealBuildTool;

public class Steamworks : ModuleRules
{
	public Steamworks(ReadOnlyTargetRules Target) : base(Target)
	{
        // The current SDK version number.
        double SteamVersionNumber = 1.53;

        // Mark the current version of the Steam SDK
        string SteamVersion = String.Format(CultureInfo.InvariantCulture, "v{0}", SteamVersionNumber).Replace(".", "");

		Type = ModuleType.External;

		PublicDefinitions.Add(String.Format(CultureInfo.InvariantCulture, "STEAM_SDK_VER=TEXT(\"{0}\")", SteamVersionNumber));
		PublicDefinitions.Add("STEAM_SDK_VER_PATH=TEXT(\"Steam" + SteamVersion + "\")");

        string SdkBase = Target.UEThirdPartySourceDirectory + "Steamworks/Steam" + SteamVersion + "/sdk";
		if (!Directory.Exists(SdkBase))
		{
			string Err = string.Format("steamworks SDK not found in {0}", SdkBase);
			System.Console.WriteLine(Err);
			throw new BuildException(Err);
		}
		
		string SteamBinariesDir = String.Format(Target.UEThirdPartyBinariesDirectory + "Steamworks/Steam{0}/", SteamVersion);
        string ArchPlatformPrefix = "";
		
		if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            ArchPlatformPrefix = "64";
            SteamBinariesDir += "Win64/";
        }

        // We do not need to explicitly link to these dlls however if they are provided in these directories, then we must take these versions.
        if (Target.Type == TargetType.Server && Target.Platform == UnrealTargetPlatform.Win64)
		{		
			string SteamClientDll = SteamBinariesDir + String.Format("steamclient{0}.dll", ArchPlatformPrefix);
			string SteamTier0Dll = SteamBinariesDir + String.Format("tier0_s{0}.dll", ArchPlatformPrefix);
			string SteamVstDll = SteamBinariesDir + String.Format("vstdlib_s{0}.dll", ArchPlatformPrefix);
			
			if(File.Exists(SteamClientDll) && File.Exists(SteamTier0Dll) && File.Exists(SteamVstDll))
			{
                System.Console.WriteLine("Linking with bundled steamclient binaries");
                RuntimeDependencies.Add(SteamClientDll);
				RuntimeDependencies.Add(SteamTier0Dll);
				RuntimeDependencies.Add(SteamVstDll);
			}
		}
		
		// Add the SDK headers
		PublicIncludePaths.Add(SdkBase + "/public");
		
		// Path for static linking (for Windows & Linux)
		string LibraryPath = SdkBase + "/redistributable_bin/";
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibraryPath += "win64/";
			
            PublicAdditionalLibraries.Add(LibraryPath + String.Format("steam_api{0}.lib", ArchPlatformPrefix));
			PublicDelayLoadDLLs.Add(String.Format("steam_api{0}.dll", ArchPlatformPrefix));
			RuntimeDependencies.Add(SteamBinariesDir + String.Format("steam_api{0}.dll", ArchPlatformPrefix));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			LibraryPath += "osx/";
			SteamBinariesDir += "Mac/";
			PublicAdditionalLibraries.Add(LibraryPath + "libsteam_api.dylib");
			// since Steam SDK 1.51, the dylib needs to be in the same directory the binary otherwise it crashes
			RuntimeDependencies.Add("$(BinaryOutputDir)/libsteam_api.dylib", LibraryPath + "libsteam_api.dylib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Linux)
		{
			if (Target.LinkType == TargetLinkType.Monolithic)
			{
				LibraryPath += "linux64/";
				PublicAdditionalLibraries.Add(LibraryPath + "libsteam_api.so");
			}
			else
			{
				LibraryPath += "linux64/libsteam_api.so";
				PublicDelayLoadDLLs.Add(LibraryPath);
			}
			
			SteamBinariesDir += String.Format("{0}", Target.Architecture);

			PrivateRuntimeLibraryPaths.Add(SteamBinariesDir);
			PublicAdditionalLibraries.Add(SteamBinariesDir + "/libsteam_api.so");
			RuntimeDependencies.Add(SteamBinariesDir + "/libsteam_api.so");
		}
	}
}
