// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HarmonixDspEditor : ModuleRules
{
	public HarmonixDspEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		//OptimizeCode = CodeOptimization.Never;

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange
		(
			new string[] {
				"Core",
				"CoreUObject",
				"DeveloperSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ContentBrowser",
				"Engine",
				"EditorWidgets",
				"EditorScriptingUtilities",
				"Harmonix",
				"HarmonixEditor",
				"HarmonixMidi",
				"HarmonixDsp",
				"UnrealEd",
				"Json",
				"AssetDefinition",
				"AssetTools",
				"PropertyEditor",
				"DetailCustomizations",
				"Slate",
				"SlateCore",
				"Settings",
				"ToolMenus",
				"InputCore"
			}
		);
	}
}
