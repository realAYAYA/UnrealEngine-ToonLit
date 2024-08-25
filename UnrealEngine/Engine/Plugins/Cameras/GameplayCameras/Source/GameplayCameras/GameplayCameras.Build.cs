// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class GameplayCameras : ModuleRules
{
	public GameplayCameras(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "Legacy"));

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"HeadMountedDisplay",
				"MovieScene",
				"MovieSceneTracks",
				"TemplateSequence"
			}
		);
	}
}
