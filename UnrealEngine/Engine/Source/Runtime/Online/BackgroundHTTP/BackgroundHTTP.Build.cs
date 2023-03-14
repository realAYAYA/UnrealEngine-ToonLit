// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BackgroundHTTP : ModuleRules
{
    public BackgroundHTTP(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicIncludePaths.AddRange(
            new string[] {
                "Runtime/Online/HTTP/Public",
            }
        );

        PrivateIncludePaths.AddRange(
            new string[] {
				"Runtime/Online/HTTP/Private",
			}
        );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "ApplicationCore",
                "Engine",
                "HTTP",
                "BackgroundHTTPFileHash",
            }
        );
    }
}
