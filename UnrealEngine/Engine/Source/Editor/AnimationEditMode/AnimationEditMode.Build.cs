// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimationEditMode : ModuleRules
{
	public AnimationEditMode(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"EditorInteractiveToolsFramework",
				"InteractiveToolsFramework",
			}
		);
	}
}