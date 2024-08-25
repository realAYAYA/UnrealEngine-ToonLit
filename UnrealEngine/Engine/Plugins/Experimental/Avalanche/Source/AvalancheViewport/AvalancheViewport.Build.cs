// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheViewport : ModuleRules
{
	public AvalancheViewport(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Avalanche",
				"AvalancheCore",
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"SlateCore",
				"UnrealEd"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AvalancheEditorCore",
				"AvalancheShapes",
				"InputCore",
				"Json",
				"Projects",
				"SettingsEditor",
				"Slate"
			}
		);
	}
}
