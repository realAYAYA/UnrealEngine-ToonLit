// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheTagEditor : ModuleRules
{
    public AvalancheTagEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "AssetDefinition",
                "AvalancheTag",
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "PropertyEditor",
                "Slate",
                "SlateCore",
                "ToolMenus",
                "UnrealEd",
            }
        );
    }
}
