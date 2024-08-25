// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StateTreeModule : ModuleRules
	{
		public StateTreeModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
				new string[] {
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"DeveloperSettings",
					"Engine",
					"AIModule",
					"GameplayTags",
					"StructUtils",
					"StructUtilsEngine",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"PropertyPath",
				}
			);

			UnsafeTypeCastWarningLevel = WarningLevel.Error;

			if (Target.bBuildEditor)
			{
				PublicDependencyModuleNames.AddRange(
					new string[] {
						"UnrealEd",
						"BlueprintGraph",
					}
				);
			}

			if (Target.Platform == UnrealTargetPlatform.Win64 && 
				(Target.Configuration != UnrealTargetConfiguration.Shipping || Target.bBuildEditor))
			{
				PublicDefinitions.Add("WITH_STATETREE_DEBUGGER=1");
				PublicDependencyModuleNames.AddRange(
					new string[]
					{
						"TraceLog",
						"TraceServices",
						"TraceAnalysis"
					}
				);
			}
			else
			{
				PublicDefinitions.Add("WITH_STATETREE_DEBUGGER=0");
			}
		}
	}
}
