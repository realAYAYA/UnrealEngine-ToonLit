// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheTransitionEditor : ModuleRules
{
    public AvalancheTransitionEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "AvalancheTag",
                "Core",
                "CoreUObject",
                "StateTreeEditorModule",
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "ApplicationCore",
                "AssetDefinition",
                "AssetTools",
                "AvalancheCore",
                "AvalancheTransition",
                "EditorStyle",
                "Engine",
                "InputCore",
                "MessageLog",
                "Projects",
                "PropertyEditor",
                "Slate",
                "SlateCore",
                "StateTreeModule",
                "StructUtils",
                "ToolMenus",
                "UnrealEd",
            }
        );

        if (Target.Platform == UnrealTargetPlatform.Win64 && (Target.Configuration != UnrealTargetConfiguration.Shipping || Target.bBuildEditor))
        {
            PrivateDefinitions.Add("WITH_STATETREE_DEBUGGER=1");
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "TraceAnalysis",
                    "TraceLog",
                    "TraceServices",
                }
            );
        }
        else
        {
            PrivateDefinitions.Add("WITH_STATETREE_DEBUGGER=0");
        }
    }
}
