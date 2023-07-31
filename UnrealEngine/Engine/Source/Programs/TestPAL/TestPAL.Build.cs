// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TestPAL : ModuleRules
{
	public TestPAL(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.Add("Runtime/Launch/Private");		// For LaunchEngineLoop.cpp include
		PrivateIncludePaths.Add("Programs/TestPAL/Private");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"ApplicationCore",
				"Projects",
			}
		);

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows) || Target.Platform == UnrealTargetPlatform.Mac ||
			Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.LinuxArm64)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"DirectoryWatcher"
				}
			);
		}

		if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.LinuxArm64)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SDL2",
				}
			);
		}
	}
}
