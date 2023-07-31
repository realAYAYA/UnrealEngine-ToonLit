// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayInsightsEditor : ModuleRules
	{
		public GameplayInsightsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "TraceServices"
            });

            PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"GameplayInsights"
			});

            if (Target.bCompileAgainstEngine)
			{
				PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"CoreUObject",
				});
			}

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorFramework",
					"UnrealEd",
				});
			}
		}
	}
}

