// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Foliage: ModuleRules
{
	public Foliage(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
                "Engine",
				"RenderCore",
			}
		);
	}
}
