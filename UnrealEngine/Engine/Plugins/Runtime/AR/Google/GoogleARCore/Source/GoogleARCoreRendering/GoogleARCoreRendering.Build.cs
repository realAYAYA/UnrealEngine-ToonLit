// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GoogleARCoreRendering : ModuleRules
{
	public GoogleARCoreRendering(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[]
		{
			"GoogleARCoreRendering/Private",
			"GoogleARCoreBase/Private",
			System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
		});
			
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
