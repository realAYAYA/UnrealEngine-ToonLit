// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64", "Linux")]
public class NetworkPredictionTests : TestModuleRules
{
	public NetworkPredictionTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"NetworkPrediction",
			}
		);

		UpdateBuildGraphPropertiesFile(new Metadata() {
			TestName = "NetworkPredictionPlugin",
			TestShortName = "Net Prediction",
			SupportedPlatforms = { UnrealTargetPlatform.Win64, UnrealTargetPlatform.Linux }
		});
	}
}
