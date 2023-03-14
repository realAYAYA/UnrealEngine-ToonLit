// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using UnrealBuildTool;

public class LowLevelTests : TestModuleRules
{
	public LowLevelTests(ReadOnlyTargetRules Target) : base(Target, true)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Cbor",
				"CoreUObject"
			});

		UpdateBuildGraphPropertiesFile(new Metadata("Self"), false);
	}
}