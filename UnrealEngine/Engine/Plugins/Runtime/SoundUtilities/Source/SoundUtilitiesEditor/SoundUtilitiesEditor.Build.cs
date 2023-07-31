// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SoundUtilitiesEditor : ModuleRules
	{
		public SoundUtilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
        {
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"EditorFramework",
					"UnrealEd",
					"AudioEditor",
					"SoundUtilities",
					"AudioMixer",
                    
                    "Slate",
                    "SlateCore",
                    "ContentBrowser",
					"ToolMenus",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
			new string[] {
					"AssetTools",
                    "AudioEditor",
            });
		}
	}
}