// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ViewportSnapping : ModuleRules
{
	public ViewportSnapping(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.Add("UnrealEd");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"EditorFramework",
				"Engine",
// 				"RenderCore",
// 				"RHI",
				"Slate",
				"UnrealEd"
			}
			);

// 		DynamicallyLoadedModuleNames.AddRange(
// 			new string[] {
// 				"MainFrame",
// 				"WorkspaceMenuStructure",
// 				"PropertyEditor"
// 			}
// 			);
	}
}
