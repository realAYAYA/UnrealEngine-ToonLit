// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TranslationEditor : ModuleRules
{
	public TranslationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("LevelEditor");
		PublicIncludePathModuleNames.Add("WorkspaceMenuStructure");
        
        PrivateIncludePathModuleNames.Add("LocalizationService");

        PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"EngineSettings",
                "InputCore",
				"Json",
                "PropertyEditor",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
                "GraphEditor",
				"SourceControl",
                "MessageLog",
                "Documentation",
				"LocalizationCommandletExecution",
				"LocalizationService",
			}
		);

        PublicDependencyModuleNames.AddRange(
			new string[] {
                "Core",
				"CoreUObject",
				"Engine",
                "Localization",
            }
        );

		DynamicallyLoadedModuleNames.AddRange(
			new string[] {
				"WorkspaceMenuStructure",
				"DesktopPlatform",
			}
		);
	}
}
