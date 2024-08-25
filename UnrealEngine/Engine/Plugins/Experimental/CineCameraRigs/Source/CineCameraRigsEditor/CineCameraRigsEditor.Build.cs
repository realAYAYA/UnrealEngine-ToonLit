// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CineCameraRigsEditor : ModuleRules
{
	public CineCameraRigsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CinematicCamera",
				"Core",
				"CoreUObject",
                "ComponentVisualizers",
				"DetailCustomizations",
				"Engine",
				"PlacementMode",
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
