// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Sequencer : ModuleRules
{
	public Sequencer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/Sequencer/Private",
				"Editor/UnrealEd/Private" // TODO: Fix this, for now it's needed for the fbx exporter
				}
			);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"TimeManagement",
				}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AppFramework", 
				"ApplicationCore",
				"AssetRegistry",
				"CinematicCamera",
				"ContentBrowser",
				"Core", 
				"CoreUObject", 
				"CurveEditor",
				"InputCore",
				"Engine", 
				"Slate", 
				"SlateCore",
				"SceneOutliner",
				"SequencerCore",
				"EditorStyle",
				"EditorFramework",
				"UnrealEd", 
				"MovieScene", 
				"MovieSceneTracks", 
				"MovieSceneTools", 
				"MovieSceneCapture", 
				"MovieSceneCaptureDialog", 
				"EditorWidgets", 
				"SequencerWidgets",
				"BlueprintGraph",
				"LevelSequence",
				"GraphEditor",
				"PropertyEditor",
				"ViewportInteraction",
				"SerializedRecorderInterface",
				"ToolMenus",
				"ToolWidgets",
				}
			);

		CircularlyReferencedDependentModules.AddRange(
			new string[] {
				"ViewportInteraction",
				}
			);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"ContentBrowser",
				"PropertyEditor",
				"Kismet",
				"LevelEditor",
				"MainFrame",
				"DesktopPlatform",
				"SerializedRecorderInterface"
				}
			);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"PropertyEditor",
				"SceneOutliner",
				"CurveEditor",
				"Analytics",
				"SequencerWidgets"
				}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"LevelEditor",
				"WorkspaceMenuStructure",
				"MainFrame",
				}
			);

		CircularlyReferencedDependentModules.Add("MovieSceneTools");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");
	}
}
