// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SequencerWidgets : ModuleRules
{
	public SequencerWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"SequencerCore",
			}
		   );

		PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "MovieScene",
            }
           );

        PublicDependencyModuleNames.AddRange(
            new string[] {
                "MovieScene",
            }
        );
        
        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
                "MovieScene",
				"Slate",
				"SlateCore",
                "MovieScene",
				"EditorFramework",
                "UnrealEd",
				"TimeManagement"
			}
		);
	}
}
