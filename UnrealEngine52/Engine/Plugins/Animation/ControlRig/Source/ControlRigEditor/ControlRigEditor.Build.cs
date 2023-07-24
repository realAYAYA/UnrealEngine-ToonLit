// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class ControlRigEditor : ModuleRules
    {
        public ControlRigEditor(ReadOnlyTargetRules Target) : base(Target)
        {
			PrivateIncludePaths.AddRange(
                new string[] {
					System.IO.Path.Combine(GetModuleDirectory("AssetTools"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("ControlRig"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("Kismet"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("MessageLog"), "Private"), //compatibility for FBX importer
					System.IO.Path.Combine(GetModuleDirectory("Persona"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("PropertyEditor"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("SceneOutliner"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("Slate"), "Private"),
					System.IO.Path.Combine(GetModuleDirectory("UnrealEd"), "Private"),
				}
			);

            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"MainFrame",
					"AppFramework",
				}
			);

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
					"CurveEditor",
					"Slate",
                    "SlateCore",
                    "InputCore",
                    "Engine",
					"EditorFramework",
					"UnrealEd",
                    "KismetCompiler",
                    "BlueprintGraph",
                    "ControlRig",
                    "ControlRigDeveloper",
                    "Kismet",
					"KismetCompiler",
                    "EditorStyle",
					"EditorWidgets",
                    "ApplicationCore",
                    "AnimationCore",
                    "PropertyEditor",
                    "AnimGraph",
                    "AnimGraphRuntime",
                    "MovieScene",
                    "MovieSceneTracks",
                    "MovieSceneTools",
                    "SequencerCore",
                    "Sequencer",
					"LevelSequenceEditor",
                    "ClassViewer",
                    "AssetTools",
                    "ContentBrowser",
					"ContentBrowserData",
                    "LevelEditor",
                    "SceneOutliner",
                    "EditorInteractiveToolsFramework",
                    "LevelSequence",
                    "GraphEditor",
                    "PropertyPath",
                    "Persona",
                    "UMG",
					"TimeManagement",
                    "PropertyPath",
					"WorkspaceMenuStructure",
					"Json",
					"DesktopPlatform",
					"ToolMenus",
                    "RigVM",
                    "RigVMDeveloper",
					"AnimationEditor",
					"MessageLog",
                    "SequencerScripting",
					"PropertyAccessEditor",
					"KismetWidgets",
					"PythonScriptPlugin",
					"AdvancedPreviewScene",
					"ToolWidgets",
                    "AnimationWidgets",
                    "ActorPickerMode",
                    "Constraints",
                    "AnimationEditMode"
				}
            );

            AddEngineThirdPartyPrivateStaticDependencies(Target, "FBX");
        }
    }
}
