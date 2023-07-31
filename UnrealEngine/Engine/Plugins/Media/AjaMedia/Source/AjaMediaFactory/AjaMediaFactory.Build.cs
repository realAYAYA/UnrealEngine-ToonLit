// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AjaMediaFactory : ModuleRules
	{
		public AjaMediaFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MediaAssets",
					"Projects",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
					"AjaMedia",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"AjaMediaFactory/Private",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
				});

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				DynamicallyLoadedModuleNames.Add("AjaMedia");
			}
		}
	}
}
