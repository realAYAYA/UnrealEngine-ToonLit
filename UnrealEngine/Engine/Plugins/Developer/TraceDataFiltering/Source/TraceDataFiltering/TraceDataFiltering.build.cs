// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TraceDataFiltering : ModuleRules
{
	public TraceDataFiltering(ReadOnlyTargetRules Target) : base(Target)
	{
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"InputCore",
					"Slate",
					"SlateCore",
					"Sockets",
					"TraceAnalysis",
					"TraceInsights",
					"TraceLog",
					"TraceServices",
				}
			);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("SharedSettingsWidgets");
		}
	}
}
