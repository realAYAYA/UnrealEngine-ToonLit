// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AssetRegistry : ModuleRules
{
	public AssetRegistry(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"TraceLog",
				"ApplicationCore",
				"Projects",
				"TelemetryUtils"
			}
			);

		if (Target.bBuildEditor == true)
		{
			PrivateIncludePathModuleNames.AddRange(new string[] { "DirectoryWatcher" });
			DynamicallyLoadedModuleNames.AddRange(new string[] { "DirectoryWatcher" });
		}

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
