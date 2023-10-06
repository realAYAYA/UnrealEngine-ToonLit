// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AnimationEditor : ModuleRules
{
	public AnimationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                
				"EditorFramework",
                "UnrealEd",
                "Persona",
                "SkeletonEditor",
                "Kismet",
                "AnimGraph",
				"ToolMenus"
            }
		);

        PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "PropertyEditor",
                "SequenceRecorder",
            }
        );

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"Persona",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "PropertyEditor",
                "SequenceRecorder",
            }
        );
    }
}
