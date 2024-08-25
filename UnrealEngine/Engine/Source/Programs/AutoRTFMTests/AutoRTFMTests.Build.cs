// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AutoRTFMTests : ModuleRules
{
	public AutoRTFMTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Catch2Extras",
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Projects",
			}
		);

		if (Target.bBuildWithEditorOnlyData)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] { "DesktopPlatform" }
			);
		}

		bAllowAutoRTFMInstrumentation = true;

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Runtime/Core/Private"
			});
	}
}
