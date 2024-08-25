// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheSVGEditor : ModuleRules
{
    public AvalancheSVGEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
	            "ActorModifierCore",
	            "AvalancheInteractiveTools",
	            "AvalancheModifiers",
                "Core",
	            "CoreUObject",
                "Engine",
                "InteractiveToolsFramework",
                "Slate",
                "SlateCore",
                "SVGImporter",
                "SVGImporterEditor",
                "ToolMenus",
            }
        );
    }
}
