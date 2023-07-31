// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ThirdPartyHelperAndDLLLoader : ModuleRules
{
	public ThirdPartyHelperAndDLLLoader( ReadOnlyTargetRules Target ) : base( Target )
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "..")
			}
		);

		PublicDependencyModuleNames.AddRange
			(
			new string[] {
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				"DirectML_1_8_0",
				"Projects"
			}
		);

		// Win64-only
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("PLATFORM_WIN64");
			PublicDefinitions.Add("PLATFORM_NNI_MICROSOFT");
		}
	}
}
