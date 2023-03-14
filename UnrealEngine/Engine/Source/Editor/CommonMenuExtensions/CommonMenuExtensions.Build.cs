// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CommonMenuExtensions : ModuleRules
{
	// TODO: Is this a minimal enough list?
	public CommonMenuExtensions(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"LauncherPlatform",
				"InputCore",
				"Slate",
				"SlateCore",
				"Engine",
				"MessageLog",
				"EditorFramework",
				"UnrealEd", 
				"RenderCore",
				"EngineSettings",
				"HierarchicalLODOutliner",
				"HierarchicalLODUtilities",
				"MaterialShaderQualitySettings",
				"ToolMenus",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
			}
		);

	}
}
