// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ReplicationSystemLowLevelTests : TestModuleRules
{
	public ReplicationSystemLowLevelTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"IrisCore",
				"ReplicationSystemTestPlugin",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"Engine",
				"RHI",
				"SlateCore",
				"InputCore",
				"ApplicationCore"
			}
		);

		UpdateBuildGraphPropertiesFile(new Metadata() {
			TestName = "ReplicationSystem",
			TestShortName = "Replication System",
			SupportedPlatforms = { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Linux, UnrealTargetPlatform.LinuxArm64 },
			PlatformRunContainerized = { { UnrealTargetPlatform.LinuxArm64, true } }
		});
	}
}
