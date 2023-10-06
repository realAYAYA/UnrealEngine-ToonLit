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
				"UncontrolledChangelists",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"SlateCore",
				"Slate",
				"UnrealEd",
			}
		);
	}
}
