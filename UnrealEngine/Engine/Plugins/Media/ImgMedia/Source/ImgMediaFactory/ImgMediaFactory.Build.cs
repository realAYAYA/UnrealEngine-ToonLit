// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ImgMediaFactory : ModuleRules
	{
		public ImgMediaFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"MediaAssets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
					"ImgMedia",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"ImgMediaFactory/Private",
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"TimeManagement",
				});

			if (Target.Type == TargetRules.TargetType.Editor)
			{
				DynamicallyLoadedModuleNames.Add("Settings");
				PrivateIncludePathModuleNames.Add("Settings");
			}

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				DynamicallyLoadedModuleNames.Add("ImgMedia");
			}
		}
	}
}
