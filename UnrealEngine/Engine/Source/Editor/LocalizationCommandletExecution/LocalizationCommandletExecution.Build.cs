// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LocalizationCommandletExecution : ModuleRules
{
	public LocalizationCommandletExecution(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"InputCore",
				"EditorFramework",
                "UnrealEd",
				"Engine",
				"DesktopPlatform",
                "SourceControl",
				"Localization",
			}
		);
	}
}
