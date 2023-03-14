// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class coremod: ModuleRules
{
	public coremod(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string CoreModVersion = "4.2.6";
		string LibraryPath = Target.UEThirdPartySourceDirectory + "coremod/coremod-" + CoreModVersion + "/";

		PublicIncludePaths.Add(LibraryPath + "include/coremod");

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicAdditionalLibraries.Add(LibraryPath + "/lib/Win64/VS2013/" + "coremod.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(LibraryPath + "/lib/Mac/libcoremodMac.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			// TODO: Do we still need this?
			// PublicLibraryPaths.Add(LibraryPath + "/lib/IOS");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
            PublicAdditionalLibraries.Add(LibraryPath + "/lib/Linux/" + Target.Architecture + "/" + "libcoremodLinux.a");
        }
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicAdditionalLibraries.Add(LibraryPath + "/lib/Android/x64/libxmp-coremod.a");
		}
	}
}
