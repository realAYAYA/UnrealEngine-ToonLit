// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateRHIRenderer : ModuleRules
{
    public SlateRHIRenderer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Slate",
				"SlateCore",
                "Engine",
                "RHI",
                "RenderCore",
				"Renderer",
                "ImageWrapper",
				"HeadMountedDisplay"
			}
		);
	}
}
