// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BlendStack : ModuleRules
{
	public BlendStack(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AnimationCore",
				"AnimGraphRuntime",
			}
		);
	}
}
