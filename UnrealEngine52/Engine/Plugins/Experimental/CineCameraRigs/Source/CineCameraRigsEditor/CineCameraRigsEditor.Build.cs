// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CineCameraRigsEditor : ModuleRules
{
	public CineCameraRigsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
        PrivateIncludePaths.Add("CineCameraRigs/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CinematicCamera",
				"Core",
				"CoreUObject",
                "ComponentVisualizers",
				"DetailCustomizations",
				"Engine",
				"SlateCore",
				"Slate",
				"UnrealEd",
				"CineCameraRigs",
				"PropertyEditor",
				"InputCore",
			});

		PublicDependencyModuleNames.AddRange(
			new string[] {
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			});
	}
}
