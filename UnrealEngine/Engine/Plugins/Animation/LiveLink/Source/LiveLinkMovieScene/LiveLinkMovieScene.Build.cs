// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkMovieScene : ModuleRules
	{
		public LiveLinkMovieScene(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"LiveLinkInterface",
				"MovieScene",
				"MovieSceneTracks",
			});

			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
			});
		}
	}
}
