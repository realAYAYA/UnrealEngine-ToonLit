// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UELibSampleRate : ModuleRules
{
	public UELibSampleRate(ReadOnlyTargetRules Target) : base(Target)
	{
		IWYUSupport = IWYUSupport.None;

        PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core"
                }
            );
	}
}
