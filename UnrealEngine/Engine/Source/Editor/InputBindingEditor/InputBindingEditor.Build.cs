// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InputBindingEditor : ModuleRules
{
	public InputBindingEditor(ReadOnlyTargetRules Target) : base(Target)
	{

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Slate",
				"SlateCore",
				"InputCore",
				"Engine",
				"EditorFramework",
				"UnrealEd",
				"PropertyEditor",
				"Settings",
				"DeveloperSettings"
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				"SettingsEditor",
			}
		);
	}
}
