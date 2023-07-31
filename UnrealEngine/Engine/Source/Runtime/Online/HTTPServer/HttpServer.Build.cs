// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HTTPServer : ModuleRules
{
    public HTTPServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
				"HTTP",
				"Sockets",
            }
        );
    }
}
