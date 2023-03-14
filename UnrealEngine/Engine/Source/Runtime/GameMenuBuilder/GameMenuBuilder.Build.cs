// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameMenuBuilder : ModuleRules
{
    public GameMenuBuilder(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] { 
					"Engine",
					"Core",
                    "CoreUObject",
					"InputCore",
					"Slate",
                    "SlateCore",
			}
		);	
	}
}
