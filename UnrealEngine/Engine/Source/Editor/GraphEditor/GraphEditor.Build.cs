// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GraphEditor : ModuleRules
{
	public GraphEditor(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicIncludePathModuleNames.AddRange(
            new string[] {                
				"ClassViewer",
				"StructViewer",
			}
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "AppFramework",
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                
				"EditorWidgets",
				"EditorFramework",
				"UnrealEd",
				"AssetRegistry",
				"Kismet",
				"KismetCompiler",
				"KismetWidgets",
				"BlueprintGraph",
				"Documentation",
				"Persona",
				"PropertyEditor",
				"RenderCore",
				"RHI",
				"ToolMenus",
				"ToolWidgets",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"ContentBrowser",
				"ClassViewer",
				"StructViewer",
			}
		);

		// Circular references that need to be cleaned up
		CircularlyReferencedDependentModules.AddRange(
		   new string[] {
				"Kismet"
		   }
	   );
	}
}
