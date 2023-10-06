// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class NiagaraEditor : ModuleRules
{
	public NiagaraEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
                "RHI",
                "Core", 
				"CoreUObject", 
				"CurveEditor",
				"DerivedDataCache",
				"ApplicationCore",
                "InputCore",
				"RenderCore",
				"Slate", 
				"SlateCore",
				"SlateNullRenderer",
				"Kismet",
                "EditorStyle",
				"VectorVM",
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
				"AssetDefinition",
				"ContentBrowser",
				"ToolWidgets",
				"AssetTools",
				"LevelSequence",
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
				"MessageLog",
				"Messaging",
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
