// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MovieRenderPipelineEditor : ModuleRules
{
	public MovieRenderPipelineEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicIncludePaths.AddRange(
			new string[] {
            }
		);
		 
		PrivateIncludePaths.AddRange(
			new string[] {
            }
        );

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"TimeManagement",
				"MovieScene",
                "MovieSceneTools",
                "MovieSceneTracks",
				"MovieRenderPipelineCore",
                "MovieRenderPipelineRenderPasses",
                "MovieRenderPipelineSettings",
				"Settings",
				"ContentBrowser",
				"PropertyEditor",
				"EditorWidgets",
				"EditorSubsystem",
				"FunctionalTesting",
            }
        );

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Kismet",
				"Slate",
				"SlateCore",
				"InputCore",
				"MovieSceneCaptureDialog",
                "MovieSceneCapture",
                "LevelSequence",
				"EditorFramework",
                "UnrealEd",
                "WorkspaceMenuStructure",				
				"LevelSequenceEditor",
				"DeveloperSettings",
				"MessageLog",
				"Sequencer",
				"ToolMenus",
				"EditorStyle",
				"Json",
				"JsonUtilities",
				"ScreenShotComparisonTools",
				"AutomationMessages",
				"ToolWidgets"
			}
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				
			}
		);
    }
}
