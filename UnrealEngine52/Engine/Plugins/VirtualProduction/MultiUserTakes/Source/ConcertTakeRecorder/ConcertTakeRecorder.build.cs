// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ConcertTakeRecorder : ModuleRules
{
	public ConcertTakeRecorder(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
			}
		);

        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Concert",
				"ConcertSyncClient",
				"ConcertSyncCore",
				"ConcertTransport",
				"EditorStyle",
				"InputCore",
				"LevelSequence",
				"Projects",
				"Slate",
				"SlateCore",
				"TakesCore",
				"TakeRecorder",
				"UnrealEd",
				"Engine"
			}
        );
    }
}
