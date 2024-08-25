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
					"ConcertClient",
					"ConcertSyncCore"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"ConcertTransport",
					"HeadMountedDisplay",
					"InputCore",
					"JsonUtilities",
					"LevelSequence",
					"MovieScene",
					"RenderCore",
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
						"EditorFramework",
						"EditorStyle",
						"EngineSettings",
						"LevelEditor",
						"Sequencer",
						"UnrealEd",
						"TypedElementRuntime",
						"TypedElementFramework",
						"ViewportInteraction",
						"VREditor",
					}
				);
			}
		}
	}
}
