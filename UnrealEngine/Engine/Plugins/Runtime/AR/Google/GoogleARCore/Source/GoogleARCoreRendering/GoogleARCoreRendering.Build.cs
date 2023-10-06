// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GoogleARCoreRendering : ModuleRules
{
	public GoogleARCoreRendering(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"Json",
			"CoreUObject",
			"Engine",
			"RHI",
			"Engine",
			"Renderer",
			"RenderCore",
			"ARUtilities",
		});
	}
}
