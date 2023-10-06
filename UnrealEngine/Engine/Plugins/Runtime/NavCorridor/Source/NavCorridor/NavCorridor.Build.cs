// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NavCorridor : ModuleRules
	{
		public NavCorridor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"AIModule",
				"Core",
				"CoreUObject",
				"Engine",
				"NavigationSystem",
			}
			);
		}
	}
}
