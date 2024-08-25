// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GameplayAbilitiesEditor : ModuleRules
	{
		public GameplayAbilitiesEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.Add("GameplayTasks");

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					// ... add private dependencies that you statically link with here ...
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"ClassViewer",
					"GameplayTags",
					"GameplayTagsEditor",
					"GameplayAbilities",
					"GameplayTasksEditor",
					"InputCore",
					"PropertyEditor",
					"Slate",
					"SlateCore",					
					"BlueprintGraph",
					"Kismet",
					"KismetCompiler",
					"GraphEditor",
					"LevelSequence",
					"MainFrame",
					"EditorFramework",
					"UnrealEd",
					"WorkspaceMenuStructure",
					"ContentBrowser",
					"EditorWidgets",
					"SourceControl",
					"SequencerCore",
					"Sequencer",
					"MovieSceneTools",
					"MovieScene",
					"DataRegistry",
					"DataRegistryEditor",
					"ToolMenus"
				}
			);
		}
	}
}
