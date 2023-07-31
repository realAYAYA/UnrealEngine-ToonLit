// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MotoSynthEditor : ModuleRules
	{
		public MotoSynthEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"UnrealEd",
					"AudioEditor",
					"MotoSynth",
					"AudioMixer",
					"ToolMenus",
					"Slate",
					"SlateCore",
					"ContentBrowser",
				}
			);

			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
					"DeveloperSettings",
				}
			);

			PrivateIncludePathModuleNames.AddRange
			(
				new string[]
				{
					"AssetTools"
				}
			);
		}
	}
}
