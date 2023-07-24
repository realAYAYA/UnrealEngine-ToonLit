// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MCPP : ModuleRules
{
	public MCPP(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Target.UEThirdPartySourceDirectory + "MCPP/mcpp-2.7.2/inc");

		string LibPath = Target.UEThirdPartySourceDirectory + "MCPP/mcpp-2.7.2/lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            LibPath += ("Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			PublicAdditionalLibraries.Add(LibPath + "/mcpp_64.lib");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(LibPath + "Mac/libmcpp.a");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			LibPath += "Linux/" + Target.Architecture.LinuxName;
			PublicAdditionalLibraries.Add(LibPath + "/libmcpp.a");
		}
	}
}

