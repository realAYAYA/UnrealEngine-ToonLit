// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertSyncClient : ModuleRules
	{
		public ConcertSyncClient(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Concert",
					"ConcertSyncCore",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ConcertTransport",
					"AssetRegistry",
					"HeadMountedDisplay",
					"InputCore",
					"MovieScene",
					"LevelSequence",
					"RenderCore",
					"TargetPlatform",
					"TimeManagement",
					"SlateCore",
					"Slate",
					"SourceControl",
					"Serialization",
                }
            );

			if (Target.bBuildEditor)
			{
				PrivateIncludePathModuleNames.AddRange(
					new string[]
					{
						"DirectoryWatcher",
						"EditorStyle",
					}
				);

				DynamicallyLoadedModuleNames.AddRange(
					new string[]
					{
						"DirectoryWatcher",
					}
				);

				PrivateDependencyModuleNames.AddRange(
					new string[] {
						"EngineSettings",
						"Sequencer",
						"EditorFramework",
						"UnrealEd",
						"ViewportInteraction",
						"LevelEditor",
						"VREditor",
						"EditorStyle",
						"TypedElementRuntime",
						"TypedElementFramework"
					}
				);
			}
		}
	}
}
