// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Constraints : ModuleRules
{
	public Constraints(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AnimationCore",
				"MovieScene"
			}
		);
	}
}