// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheMaskEditor : ModuleRules
{
	public AvalancheMaskEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
            });

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ActorModifierCore",
				"Avalanche",
				"AvalancheEditorCore",
				"AvalancheMask",
				"AvalancheModifiers",
				"AvalancheShapes",
				"CoreUObject",
				"DynamicMaterialEditor",
				"EditorFramework",
				"EditorSubsystem",
				"Engine",
				"GeometryMask",
				"GeometryMaskEditor",
				"InputCore",
				"LevelEditor",
				"Projects",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"TypedElementFramework",
				"TypedElementRuntime",
				"UnrealEd",
			});
    }
}
