// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnsavedAssetsTracker : ModuleRules
{
	public UnsavedAssetsTracker(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
				"EditorSubsystem",
				"SourceControl",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"SlateCore",
				"Slate",
				"UnrealEd",
			}
		);
	}
}
