// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ClonerEffectorEditor : ModuleRules
{
    public ClonerEffectorEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
	        new string[]
	        {
		        "Core",
		        "CoreUObject",
		        "Engine",
		        "Slate",
		        "SlateCore",
		        "UnrealEd",
	        }
        );

        PrivateDependencyModuleNames.AddRange(
	        new string[]
	        {
		        "ClonerEffector",
		        "InputCore",
		        "Projects",
	        }
        );

        ShortName = "CEEditor";
    }
}