// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaCompositing : ModuleRules
	{
		public MediaCompositing(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"MovieScene",
					"MovieSceneTracks",
					"TimeManagement",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"Media",
					"MediaAssets",
					"RenderCore",
					"RHI",
				});

			PrivateIncludePaths.AddRange(
				new string[] {
					"MediaCompositing/Private",
					"MediaCompositing/Private/MovieScene",
				});
		}
	}
}
