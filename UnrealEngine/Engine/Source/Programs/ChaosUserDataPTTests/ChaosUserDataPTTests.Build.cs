// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosUserDataPTTests : TestModuleRules
{
	public ChaosUserDataPTTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				//"CoreUObject",
				"Chaos",
				"ChaosUserDataPT"
			});

		//PrivateIncludePaths.Add("FortniteGame/Private");
		//PublicIncludePaths.Add("FortniteGame/Public");
	}
}