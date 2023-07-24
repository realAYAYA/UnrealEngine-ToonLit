// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AudioSynesthesiaEditor : ModuleRules
{
	public AudioSynesthesiaEditor(ReadOnlyTargetRules Target) : base(Target)
	{

		//bFasterWithoutUnity = true;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"EditorFramework",
				"UnrealEd",
				"AudioSynesthesia",
				"AudioAnalyzer",
                "InputCore",
                "Slate",
                "SlateCore",
                "EditorStyle"
            }
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools"
			});
	}
}
