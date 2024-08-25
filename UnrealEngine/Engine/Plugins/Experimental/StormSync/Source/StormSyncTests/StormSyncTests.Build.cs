// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StormSyncTests : ModuleRules
{
	public StormSyncTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Projects",
				"StormSyncCore",
				"StormSyncDrives",
			}
		);
	}
}
