// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FileUtilities : ModuleRules
{
	public FileUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "Core"});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "libzip" });
		}
	}
}
