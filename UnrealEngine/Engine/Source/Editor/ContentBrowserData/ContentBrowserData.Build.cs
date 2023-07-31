// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ContentBrowserData : ModuleRules
{
	public ContentBrowserData(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"EditorSubsystem",
				"EditorFramework",
				"UnrealEd",
				"Projects",
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"CollectionManager",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Slate",
				"SlateCore",
				"InputCore",
			}
		);
	}
}
