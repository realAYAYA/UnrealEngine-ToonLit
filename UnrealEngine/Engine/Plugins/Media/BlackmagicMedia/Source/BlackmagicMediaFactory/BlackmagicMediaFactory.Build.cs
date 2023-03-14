// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class BlackmagicMediaFactory : ModuleRules
	{
		public BlackmagicMediaFactory(ReadOnlyTargetRules Target) : base(Target)
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
					"BlackmagicMedia",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"BlackmagicMediaFactory/Private",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
				});

			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				DynamicallyLoadedModuleNames.Add("BlackmagicMedia");
			}
		}
	}
}
