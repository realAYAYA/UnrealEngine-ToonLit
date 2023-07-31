// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TranslationEditor : ModuleRules
{
	public TranslationEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.AddRange(
			new string[] {
				"Editor/PropertyEditor/Private",
			}
		);

		PublicIncludePathModuleNames.Add("LevelEditor");
		PublicIncludePathModuleNames.Add("WorkspaceMenuStructure");
        
        PrivateIncludePathModuleNames.Add("LocalizationService");

        PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DesktopPlatform",
                "MessageLog",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
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
                "Localization",
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
