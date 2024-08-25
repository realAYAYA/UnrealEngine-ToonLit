// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DerivedDataEditor : ModuleRules
{
	public DerivedDataEditor(ReadOnlyTargetRules Target)
		 : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"OutputLog",
				"DerivedDataCache",
				"EditorSubsystem",
				"WorkspaceMenuStructure",
				"MessageLog",
				"ToolWidgets",
				"UATHelper",
				"Zen"
			});

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"MainFrame",
			});


		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			});
		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
