// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ImgMediaEngine : ModuleRules
	{
		public ImgMediaEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"MediaAssets",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
					"ImgMedia",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"ImgMediaEngine/Private",
					"ImgMediaEngine/Private/Unreal",
				});

			// Are we using the engine?
			if (Target.bCompileAgainstEngine)
			{
				PublicDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"Engine",
				});
			}

			// Are we using the editor?
			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnrealEd",
				});
			}

		}
	}
}
