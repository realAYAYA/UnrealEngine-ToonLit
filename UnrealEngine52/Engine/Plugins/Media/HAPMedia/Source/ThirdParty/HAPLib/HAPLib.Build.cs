// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.IO;
using UnrealBuildTool;

public class HAPLib : ModuleRules
{
    public HAPLib(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string IncPath = Path.Combine(ModuleDirectory, "include");
            PublicSystemIncludePaths.Add(IncPath);

            string LibPath = Path.Combine(ModuleDirectory, "lib");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "HAP.lib"));
        }
    }
}
	