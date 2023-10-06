// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClothingSystemRuntimeCommon : ModuleRules
{
	public ClothingSystemRuntimeCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		SetupModulePhysicsSupport(Target);

		PublicIncludePathModuleNames.Add("ClothingSystemRuntimeInterface");

		PublicDependencyModuleNames.AddRange(
		new string[] {
			"Core",
			"CoreUObject",
			"ClothingSystemRuntimeInterface"
		}
		);

		PrivateDependencyModuleNames.AddRange(
		new string[]
		{
			"Engine",
			"Slate"
		}
		);
	}
}
