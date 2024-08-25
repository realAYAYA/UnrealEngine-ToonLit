// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GameplayGraphTests : TestModuleRules
{
	public GameplayGraphTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"GameplayGraph"
			});

		UpdateBuildGraphPropertiesFile(new Metadata() {
			TestName = "GameplayGraph",
			TestShortName = "GameplayGraph"
		});
	}
}