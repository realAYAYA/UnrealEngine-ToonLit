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

		if (Target.bBuildWithEditorOnlyData)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] { "DesktopPlatform" }
				);
		}

		UpdateBuildGraphPropertiesFile(new Metadata("Self"), false);
	}
}