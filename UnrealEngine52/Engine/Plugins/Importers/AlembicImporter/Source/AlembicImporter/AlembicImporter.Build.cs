// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AlembicImporter : ModuleRules
{
    public AlembicImporter(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
                "InputCore",
				"Slate",
				"SlateCore",
				"EditorFramework",
				"UnrealEd",
                "MainFrame",
                "PropertyEditor",
                "AlembicLibrary",
                "GeometryCache",
                "RenderCore",
                "RHI"
            }
		);
	}
}
