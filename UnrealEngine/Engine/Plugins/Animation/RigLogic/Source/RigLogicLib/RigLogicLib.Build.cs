// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class RigLogicLib : ModuleRules
{
    public string ModulePath
    {
        get { return ModuleDirectory; }
    }

    public RigLogicLib(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                    "Core",
                    "CoreUObject",
                    "Engine",
                    "ControlRig"
            }
        );

        if (Target.Type == TargetType.Editor)
        {
            PublicDependencyModuleNames.Add("UnrealEd");
            PublicDependencyModuleNames.Add("EditorFramework");
        }

        Type = ModuleType.CPlusPlus;

        if (Target.LinkType != TargetLinkType.Monolithic)
        {
            PrivateDefinitions.Add("RL_BUILD_SHARED=1");
        }

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDefinitions.Add("TRIO_WINDOWS_FILE_MAPPING_AVAILABLE=1");
            PrivateDefinitions.Add("TRIO_CUSTOM_WINDOWS_H=\"WindowsPlatformUE.h\"");
        }

        if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.LinuxArm64)
        {
            PrivateDefinitions.Add("TRIO_MREMAP_AVAILABLE=1");
        }

        if (Target.Platform == UnrealTargetPlatform.Linux ||
                Target.Platform == UnrealTargetPlatform.LinuxArm64 ||
                Target.Platform == UnrealTargetPlatform.Mac)
        {
            PrivateDefinitions.Add("TRIO_MMAP_AVAILABLE=1");
        }

        if (Target.Architecture.StartsWith("x86_64"))
        {
            PrivateDefinitions.Add("RL_BUILD_WITH_SSE=1");
        }
    }
}
