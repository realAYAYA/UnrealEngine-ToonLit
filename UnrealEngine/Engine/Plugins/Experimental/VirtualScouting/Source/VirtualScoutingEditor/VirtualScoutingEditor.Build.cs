// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualScoutingEditor : ModuleRules
{
	public VirtualScoutingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"VirtualScoutingOpenXR/Private",
			}
		);

		PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(new string[]
			{
				"OpenXRHMD",
				"UnrealEd",
				"VREditor",
				"VirtualScoutingOpenXR",
			}
		);
	}
}
