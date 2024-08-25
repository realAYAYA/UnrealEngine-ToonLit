// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class OptimusEditor : ModuleRules
    {
        public OptimusEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
					"Core",
					"CoreUObject",
					"ApplicationCore",
					"SlateCore",
					"Slate",
					"GraphEditor",
					"Engine",
					"EditorFramework",
					"UnrealEd",
					"AssetTools",
					"AdvancedPreviewScene",
					"InputCore",
					"RHI", 
					"ToolMenus",
					"ComputeFramework",
					"OptimusCore",
					"OptimusDeveloper",
					"BlueprintGraph",		// For the graph pin colors
					"Persona",
					"MessageLog",
					"PropertyEditor",
					"KismetCompiler",
					"SourceCodeAccess"
				}
			);

        }
    }
}
