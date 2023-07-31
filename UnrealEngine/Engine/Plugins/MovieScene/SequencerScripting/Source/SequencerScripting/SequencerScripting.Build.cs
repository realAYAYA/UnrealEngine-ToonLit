// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SequencerScripting : ModuleRules
{
	public SequencerScripting(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
            }
		);

		PrivateIncludePaths.AddRange(
			new string[] {
				"SequencerScripting/Private",
				"SequencerScripting/Private/ExtensionLibraries",
                "SequencerScripting/Private/KeysAndChannels",
            }
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"TimeManagement",
				"MovieScene",
                "MovieSceneTracks",
            }
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "LevelSequence",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
			}
		);
	}
}
