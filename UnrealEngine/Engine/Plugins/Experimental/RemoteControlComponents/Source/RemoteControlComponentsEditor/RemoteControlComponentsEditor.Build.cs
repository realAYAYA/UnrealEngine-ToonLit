// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RemoteControlComponentsEditor : ModuleRules
{
    public RemoteControlComponentsEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "EditorSubsystem",
                "Projects",
                "RemoteControl",
				"RemoteControlComponents",
                "Slate",
                "SlateCore",
                "UnrealEd"
            }
        );
    }
}