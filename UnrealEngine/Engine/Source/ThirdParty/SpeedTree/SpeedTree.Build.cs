// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;
using System.IO;

public class SpeedTree : ModuleRules
{
	public SpeedTree(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		var bPlatformAllowed = ((Target.Platform == UnrealTargetPlatform.Win64) ||
								(Target.Platform == UnrealTargetPlatform.Mac) || Target.IsInPlatformGroup(UnrealPlatformGroup.Unix));

		if (bPlatformAllowed &&
			Target.bCompileSpeedTree)
		{
			PublicDefinitions.Add("WITH_SPEEDTREE=1");
			PublicDefinitions.Add("SPEEDTREE_KEY=INSERT_KEY_HERE");

            string SpeedTreePath = Target.UEThirdPartySourceDirectory + "SpeedTree/SpeedTreeSDK-v7.0/";
            PublicSystemIncludePaths.Add(SpeedTreePath + "Include");

            string SpeedTree8Path = Target.UEThirdPartySourceDirectory + "SpeedTree/SpeedTreeDataBuffer/";
            PublicSystemIncludePaths.Add(SpeedTree8Path);

            if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/Windows/VC14.x64/SpeedTreeCore_Windows_v7.0_VC14_MTDLL64_Static_d.lib");
				}
				else
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/Windows/VC14.x64/SpeedTreeCore_Windows_v7.0_VC14_MTDLL64_Static.lib");
				}
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/MacOSX/Debug/libSpeedTreeCore.a");
				}
				else
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/MacOSX/Release/libSpeedTreeCore.a");
				}
			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				if (Target.LinkType == TargetLinkType.Monolithic)
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/Linux/" + Target.Architecture.LinuxName + "/Release/libSpeedTreeCore.a");
				}
				else
				{
					PublicAdditionalLibraries.Add(SpeedTreePath + "Lib/Linux/" + Target.Architecture.LinuxName + "/Release/libSpeedTreeCore_fPIC.a");
				}
			}
		}
	}
}

