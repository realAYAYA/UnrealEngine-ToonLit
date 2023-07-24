// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StringTableEditor : ModuleRules
{
    public StringTableEditor(ReadOnlyTargetRules Target)
         : base(Target)
    {
        PrivateDependencyModuleNames.AddRange(
            new string[] {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "Slate",
                "SlateCore",
                "DesktopPlatform",
				"EditorFramework",
                "UnrealEd",
				"AssetTools",
				"AssetDefinition",
			});

        DynamicallyLoadedModuleNames.Add("WorkspaceMenuStructure");
    }
}
