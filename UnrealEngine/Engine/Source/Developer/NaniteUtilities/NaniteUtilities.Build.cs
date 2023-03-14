// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NaniteUtilities : ModuleRules
{
	public NaniteUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Engine",
			}
		);
	}
}
