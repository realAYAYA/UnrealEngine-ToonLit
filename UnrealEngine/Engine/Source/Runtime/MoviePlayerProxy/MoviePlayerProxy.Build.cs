// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MoviePlayerProxy : ModuleRules
{
	public MoviePlayerProxy(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
            new string[]
			{
                    "Core",
			}
			);
	}
}
