// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StorageServerClient : ModuleRules
{
	public StorageServerClient(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Sockets",
                "CookOnTheFly",
                "Json"
            }
        );
    }
}
