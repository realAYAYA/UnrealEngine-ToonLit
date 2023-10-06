// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class nvTriStrip : ModuleRules
{
	public nvTriStrip(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string NvTriStripPath = Target.UEThirdPartySourceDirectory + "nvTriStrip/nvTriStrip-1.0.0/";
        PublicSystemIncludePaths.Add(NvTriStripPath + "Inc");

		string NvTriStripLibPath = NvTriStripPath + "Lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			NvTriStripLibPath += "Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(NvTriStripLibPath + "/nvTriStripD_64.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add(NvTriStripLibPath + "/nvTriStrip_64.lib");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
			PublicAdditionalLibraries.Add(NvTriStripLibPath + "Mac/libnvtristrip" + Postfix + ".a");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
            string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
            PublicAdditionalLibraries.Add(NvTriStripLibPath + "Linux/" + Target.Architecture.LinuxName + "/libnvtristrip" + Postfix + ".a");
        }
	}
}
