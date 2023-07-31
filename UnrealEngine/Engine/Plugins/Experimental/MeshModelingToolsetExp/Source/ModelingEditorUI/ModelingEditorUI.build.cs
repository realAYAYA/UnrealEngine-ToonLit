// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ModelingEditorUI : ModuleRules
{
	public ModelingEditorUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
            );
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "ApplicationCore",
                "Slate",
                "SlateCore",
                "Engine",
                "InputCore",
				"EditorFramework",
				"UnrealEd",
                "ContentBrowser",
				"ContentBrowserData",
                "LevelEditor",
				"StatusBar",
                "Projects",
				"ToolWidgets",
				"EditorWidgets",
				"DeveloperSettings"
			}
			);


		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
