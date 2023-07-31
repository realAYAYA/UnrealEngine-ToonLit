// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DMXPixelMappingRenderer : ModuleRules
{
    public DMXPixelMappingRenderer(ReadOnlyTargetRules Target) : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "Engine",
                "CoreUObject",
				"Projects",
				"RenderCore",
				"RHI",
				"Slate",
				"SlateCore",
				"UMG",
                "DMXRuntime"
			}
        );


        PublicDependencyModuleNames.AddRange(
            new string[] {
                "Core",
				"CoreUObject",
			}
		);
    }
}
