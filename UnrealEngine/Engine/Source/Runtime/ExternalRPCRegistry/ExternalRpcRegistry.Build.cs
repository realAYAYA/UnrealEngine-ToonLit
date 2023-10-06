// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ExternalRpcRegistry : ModuleRules
{
    public ExternalRpcRegistry(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
				"Json",
                "HTTPServer"
            }
        );
    }
}
