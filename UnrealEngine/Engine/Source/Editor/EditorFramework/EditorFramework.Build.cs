// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorFramework : ModuleRules
{
	public EditorFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"RenderCore",
				"Slate",
				"SlateCore",
				"WorkspaceMenuStructure"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"EditorSubsystem",
				"InteractiveToolsFramework",
				"TypedElementFramework",
				"TypedElementRuntime",
			}
		);
	}
}
