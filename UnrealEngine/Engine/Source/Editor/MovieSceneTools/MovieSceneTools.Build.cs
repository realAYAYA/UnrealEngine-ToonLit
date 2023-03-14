// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieSceneTools : ModuleRules
{
	public MovieSceneTools(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
            new string[] {
				System.IO.Path.Combine(GetModuleDirectory("UnrealEd"), "Private"), //compatibility for FBX importer
				System.IO.Path.Combine(GetModuleDirectory("SceneOutliner"), "Private"),
			}
        );

		OverridePackageType = PackageOverrideType.EngineDeveloper;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
                "MovieSceneCapture",
				"UnrealEd",
				"Sequencer",
                "EditorWidgets",
				"SequencerCore",
				"Constraints"
            }
        );

		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "ActorPickerMode",
				"AnimationBlueprintLibrary",
				"AppFramework",
				"CinematicCamera",
				"ClassViewer",
				"DataLayerEditor",
                "CurveEditor",
                "DesktopPlatform",
                "Json",
                "EditorFramework",
                "JsonUtilities",
				"LevelSequence",
                "LiveLinkInterface",
                "MessageLog",
				"MovieScene",
				"MovieSceneTracks",
				"BlueprintGraph",
				"Kismet",
				"KismetCompiler",
                "GraphEditor",
                "ContentBrowser",
				"Slate",
				"SlateCore",
				"SceneOutliner",
                "EditorStyle",
				"PropertyEditor",
                "MaterialEditor",
				"RenderCore",
				"RHI",
				"SequenceRecorder",
				"TimeManagement",
                "AnimationCore",
				"TimeManagement",
                "XmlParser",
				"ToolMenus",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
                "AssetRegistry",
				"AssetTools",
				"Sequencer",
                "Settings",
				"SceneOutliner",
                "MainFrame",
				"EditorFramework",
                "UnrealEd",
                "Analytics",
            }
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
                "AssetRegistry",
				"AssetTools",
			    "MainFrame",
			}
		);

        CircularlyReferencedDependentModules.AddRange(
            new string[] {
                "Sequencer",
            }
        );

        AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");
    }
}
