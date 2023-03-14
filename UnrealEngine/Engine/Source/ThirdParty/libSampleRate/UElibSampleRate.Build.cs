// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UELibSampleRate : ModuleRules
{
	public UELibSampleRate(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core"
                }
            );
	}
}
