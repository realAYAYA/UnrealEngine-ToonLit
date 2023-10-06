// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ImageWriteQueue : ModuleRules
{
	public ImageWriteQueue(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ImageCore",
				"ImageWrapper",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
				"RenderCore",
				"RHI",
			}
		);
	}
}
