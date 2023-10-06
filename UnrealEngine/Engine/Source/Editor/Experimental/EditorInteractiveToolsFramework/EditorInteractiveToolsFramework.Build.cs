// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorInteractiveToolsFramework : ModuleRules
{
	public EditorInteractiveToolsFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"TypedElementFramework",
				// ... add other public dependencies that you statically link with here ...
			}
			);			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"RenderCore",
				"Slate",
                "SlateCore",
                "InputCore",
				"EditorFramework",
				"UnrealEd",
                "ContentBrowser",
                "LevelEditor",
                "ApplicationCore",
                "InteractiveToolsFramework",
				"MeshDescription",
				"StaticMeshDescription",
                "EditorSubsystem",
                "TypedElementRuntime"

				// ... add private dependencies that you statically link with here ...	
			}
            );
	}
}
