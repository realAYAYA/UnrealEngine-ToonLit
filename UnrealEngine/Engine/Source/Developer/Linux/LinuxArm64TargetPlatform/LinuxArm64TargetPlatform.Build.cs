// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LinuxArm64TargetPlatform : ModuleRules
{
    public LinuxArm64TargetPlatform(ReadOnlyTargetRules Target) : base(Target)
    {
        BinariesSubFolder = "LinuxArm64";

        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"DesktopPlatform",
				"TargetPlatform",
			}
        );

        if (Target.bCompileAgainstEngine)
        {
            PrivateDependencyModuleNames.AddRange(new string[] {
					"Engine"
				}
            );

            PrivateIncludePathModuleNames.Add("TextureCompressor");
        }

		PrivateIncludePathModuleNames.Add("LinuxTargetPlatform");
    }
}
