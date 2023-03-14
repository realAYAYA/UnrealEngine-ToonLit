// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class GLTFImporter : ModuleRules
    {
        public GLTFImporter(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                    "Engine",
					"EditorFramework",
                    "UnrealEd",
                    "MeshDescription",
					"StaticMeshDescription",
                    "MeshUtilities",
                    "MessageLog",
                    "Json",
                    "MaterialEditor",
                    "Slate",
                    "SlateCore",
                    "MainFrame",
                    "InputCore",
					"GLTFCore",
                }
                );
        }
    }
}
