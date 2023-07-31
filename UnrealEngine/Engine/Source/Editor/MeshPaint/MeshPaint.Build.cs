// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MeshPaint : ModuleRules
{
    public MeshPaint(ReadOnlyTargetRules Target) : base(Target)
    {
		PrivateIncludePathModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
                "AssetTools"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "AppFramework",
                "Core", 
                "CoreUObject",
                "DesktopPlatform",
                "Engine", 
                "InputCore",
                "RenderCore",
                "RHI",
                "Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
                "MeshDescription",
				"StaticMeshDescription",
                "SourceControl",
                "PropertyEditor",
                "MainFrame",
            }
        );

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"AssetTools",
				"LevelEditor"
            });

		DynamicallyLoadedModuleNames.AddRange(
            new string[] {
                "AssetRegistry",
                "AssetTools"
            }
        );
    }
}
