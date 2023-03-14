// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class NiagaraEditor : ModuleRules
{
	public NiagaraEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(new string[] {
			"NiagaraEditor/Private",
			"NiagaraEditor/Private/Toolkits",
			"NiagaraEditor/Private/Widgets",
			"NiagaraEditor/Private/Sequencer/NiagaraSequence",
			"NiagaraEditor/Private/ViewModels",
			"NiagaraEditor/Private/TypeEditorUtilities",
			Path.Combine(GetModuleDirectory("GraphEditor"), "Private"),
			Path.Combine(GetModuleDirectory("Niagara"), "Private"),
			Path.Combine(GetModuleDirectory("PropertyEditor"), "Private"),
			Path.Combine(GetModuleDirectory("Renderer"), "Private"),
		});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine",
                "RHI",
                "Core", 
				"CoreUObject", 
				"CurveEditor",
				"ApplicationCore",
                "InputCore",
				"RenderCore",
				"Slate", 
				"SlateCore",
				"SlateNullRenderer",
				"Kismet",
                "EditorStyle",
				"UnrealEd", 
				"VectorVM",
                "NiagaraCore",
                "Niagara",
                "NiagaraShader",
                "MovieScene",
				"Sequencer",
				"TimeManagement",
				"PropertyEditor",
				"GraphEditor",
                "ShaderFormatVectorVM",
                "TargetPlatform",
                "DesktopPlatform",
                "AppFramework",
				"MovieSceneTools",
                "MovieSceneTracks",
                "AdvancedPreviewScene",
				"Projects",
                "MainFrame",
				"ToolMenus",
				"Renderer",
				"EditorWidgets",
				"Renderer",
				"DeveloperSettings",
				"PythonScriptPlugin",
				"ImageWrapper",
			}
		);

		if (Target.bBuildTargetDeveloperTools)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"SessionServices",
					"SessionFrontend"
				}
			);
		}

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Engine",
				"MessageLog",
				"Messaging",
				"AssetTools",
				"ContentBrowser",
                "DerivedDataCache",
                "LevelEditor",
				"WorkspaceMenuStructure"
			}
        );

		PublicDependencyModuleNames.AddRange(
            new string[] {
                "NiagaraCore",
                "NiagaraShader",
                "Engine",
                "NiagaraCore",
                "Niagara",
				"EditorFramework",
                "UnrealEd",
            }
        );

		PublicIncludePathModuleNames.AddRange(
            new string[] {
				"Engine",
				"Niagara"
			}
		);

        DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "WorkspaceMenuStructure",
                }
            );
	}
}
