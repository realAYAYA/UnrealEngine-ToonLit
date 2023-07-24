// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OneSkyLocalizationService : ModuleRules
{
    public OneSkyLocalizationService(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
                "Engine",
                "InputCore",
				"Slate",
				"SlateCore",
                "LocalizationService",
                "Json",
                "HTTP",
                "Serialization",
				"Localization",
				"LocalizationCommandletExecution",
				"MainFrame",
			}
		);
	}
}
