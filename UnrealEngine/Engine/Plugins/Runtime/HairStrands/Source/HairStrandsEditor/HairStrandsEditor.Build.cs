// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class HairStrandsEditor : ModuleRules
	{
		public HairStrandsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"EditorFramework",
					"Engine",
					"GeometryCache",
					"HairStrandsCore",
					"InputCore",
					"MainFrame",
					"Slate",
					"SlateCore",
					"Projects",
					"ToolMenus",
					"ContentBrowser",
					"UnrealEd",
					"AssetDefinition",
					"AssetTools",
					"EditorInteractiveToolsFramework",
					"AdvancedPreviewScene",
					"InputCore",
					"Renderer",
					"PropertyEditor",
					"RHI",
					"LevelSequence",
					"MovieScene",
					"MovieSceneTools",
					"SequencerCore",
					"Sequencer",
					"HairCardGeneratorFramework",
					"CommonMenuExtensions",
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Analytics"
				});

			AddEngineThirdPartyPrivateStaticDependencies(Target,
				"FBX"
			);
		}
	}
}