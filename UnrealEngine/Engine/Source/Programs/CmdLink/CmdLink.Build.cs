// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CmdLink : ModuleRules
{
    public CmdLink(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
	            "Core",
            }
        );
        
        PrivateIncludePathModuleNames.AddRange(
	        new string[] {
		        "Launch",
		        "TargetPlatform",
	        });
    }
}