// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheEffectorsEditor : ModuleRules
{
	public AvalancheEffectorsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"AvalancheShapesEditor",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AvalancheComponentVisualizers",
				"AvalancheCore",
				"AvalancheEffectors",
				"AvalancheInteractiveTools",
				"AvalancheShapes",
				"ClonerEffector",
				"ClonerEffectorEditor",
				"ComponentVisualizers",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"InteractiveToolsFramework",
				"Projects",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);
	}
}
