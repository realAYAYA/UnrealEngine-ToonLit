// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class LidarPointCloudEditor : ModuleRules
    {
        public LidarPointCloudEditor(ReadOnlyTargetRules Target) : base(Target)
        {
            PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
				"EditorFramework",
                "UnrealEd",
                "KismetWidgets",
                "PropertyEditor",
                "ApplicationCore",
                "InputCore",
                "MainFrame",
                "LidarPointCloudRuntime",
                "Projects",
                "UMG",
                "Core",
                "CoreUObject",
                "Engine",
                "RenderCore",
                "RHI",
                "AdvancedPreviewScene",
                "EditorInteractiveToolsFramework",
                "InteractiveToolsFramework",
                "StatusBar",
                "MeshDescription",
                "StaticMeshDescription"
                }
            );
        }
    }
}