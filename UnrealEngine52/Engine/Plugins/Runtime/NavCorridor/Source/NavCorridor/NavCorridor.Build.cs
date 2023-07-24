// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NavCorridor : ModuleRules
	{
		public NavCorridor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicIncludePaths.AddRange(
				new string[] {
				ModuleDirectory + "/Public",
				}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"AIModule",
				"Core",
				"CoreUObject",
				"Engine",
				"NavigationSystem",
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
			}
			);
		}

	}
}
