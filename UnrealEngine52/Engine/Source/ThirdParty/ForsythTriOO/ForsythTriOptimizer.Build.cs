// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ForsythTriOptimizer : ModuleRules
{
	public ForsythTriOptimizer(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string ForsythTriOptimizerPath = Target.UEThirdPartySourceDirectory + "ForsythTriOO/";
        PublicSystemIncludePaths.Add(ForsythTriOptimizerPath + "Src");

		string ForsythTriOptimizerLibPath = ForsythTriOptimizerPath + "Lib/";

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
            ForsythTriOptimizerLibPath += "Win64/VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				PublicAdditionalLibraries.Add(ForsythTriOptimizerLibPath + "/ForsythTriOptimizerD_64.lib");
			}
			else
			{
				PublicAdditionalLibraries.Add(ForsythTriOptimizerLibPath + "/ForsythTriOptimizer_64.lib");
			}
		}
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
            PublicAdditionalLibraries.Add(ForsythTriOptimizerLibPath + "Mac/libForsythTriOptimizer" + Postfix + ".a");
        }
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
        {
            string Postfix = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "d" : "";
            PublicAdditionalLibraries.Add(ForsythTriOptimizerLibPath + "Linux/" + Target.Architecture.LinuxName + "/libForsythTriOptimizer" + Postfix + ".a");
        }
	}
}
