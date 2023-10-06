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

            string LibPath = Path.Combine(ModuleDirectory, "lib/Win64");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "HAP.lib"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string IncPath = Path.Combine(ModuleDirectory, "include");
            PublicSystemIncludePaths.Add(IncPath);

            string LibPath = Path.Combine(ModuleDirectory, "lib/Mac");
            PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libHAP.a"));
        }
    }
}
