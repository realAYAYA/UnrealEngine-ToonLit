// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InterchangeCore : ModuleRules
{
	public InterchangeCore(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Json"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"SlateCore",
			}
		);
	}
}
