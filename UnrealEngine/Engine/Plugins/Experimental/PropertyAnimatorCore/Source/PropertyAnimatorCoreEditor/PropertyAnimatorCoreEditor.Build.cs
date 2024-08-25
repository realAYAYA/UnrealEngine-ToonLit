// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PropertyAnimatorCoreEditor : ModuleRules
{
    public PropertyAnimatorCoreEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
	        new string[]
	        {
		        "Core"
	        }
        );

        PrivateDependencyModuleNames.AddRange(
	        new string[]
	        {
		        "ApplicationCore",
		        "CoreUObject",
		        "EditorSubsystem",
		        "EditorWidgets",
		        "Engine",
		        "InputCore",
		        "OperatorStackEditor",
		        "Projects",
				"PropertyAnimatorCore",
				"PropertyEditor",
		        "SlateCore",
		        "Slate",
		        "StructUtils",
		        "ToolMenus",
		        "UnrealEd"
	        }
        );

		ShortName = "PropAnimCoreEd";
    }
}