// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StaticMeshEditorModeling : ModuleRules
{
	public StaticMeshEditorModeling(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{			
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"InputCore",
				"CoreUObject",
				"EditorInteractiveToolsFramework",				
				"EditorFramework",
				"InteractiveToolsFramework",
				"MeshLODToolset",
				"ModelingComponentsEditorOnly",
				"ModelingToolsEditorMode",
				"Slate",
				"SlateCore",
				"StaticMeshEditor",
				"MeshModelingToolsExp",
				"ToolMenus",
				"UnrealEd",
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
