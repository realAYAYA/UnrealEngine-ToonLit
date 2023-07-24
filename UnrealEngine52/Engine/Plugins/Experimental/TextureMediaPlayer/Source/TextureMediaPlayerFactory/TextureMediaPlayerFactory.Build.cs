// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class TextureMediaPlayerFactory : ModuleRules
	{
		public TextureMediaPlayerFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"MediaAssets",
                });

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Media",
				});

			PublicIncludePathModuleNames.Add("TextureMediaPlayer");
		}
	}
}
