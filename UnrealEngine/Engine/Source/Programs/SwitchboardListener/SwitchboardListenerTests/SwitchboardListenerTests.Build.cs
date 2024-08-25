// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;


public class SwitchboardListenerTests : TestModuleRules
{
	public SwitchboardListenerTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] {
			"SblCore",

			"JWT",
		});

		PrivateIncludePathModuleNames.Add("LowLevelTestsRunner");

		UpdateBuildGraphPropertiesFile(new Metadata() { TestName = "SwitchboardListener", TestShortName = "SwitchboardListener" });
	}
}
