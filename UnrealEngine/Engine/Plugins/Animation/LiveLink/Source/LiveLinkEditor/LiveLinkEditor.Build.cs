// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkEditor : ModuleRules
	{
		public LiveLinkEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"LiveLinkAnimationCore",
					"LiveLinkInterface",
					"LiveLink",
					"SlateCore",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimGraph",
					"AssetRegistry",
					"BlueprintGraph",
					"ClassViewer",
					"Core",
					"CoreUObject",
					"DetailCustomizations",
					"EditorStyle",
					"EditorWidgets",
					"Engine",
					"InputCore",
					"KismetCompiler",
					"GraphEditor",
					"LiveLinkComponents",
					"LiveLinkGraphNode",
					"LiveLinkMessageBusFramework",
					"LiveLinkMovieScene",
					"MessageLog",
					"Persona",
					"Projects",
					"PropertyEditor",
					"Settings",
					"Sequencer",
					"Slate",
					"TimeManagement",
					"EditorFramework",
					"UnrealEd",
					"WorkspaceMenuStructure",
					"ToolWidgets",
				}
			);
		}
	}
}
