// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SharedSettingsWidgets : ModuleRules
{
	public SharedSettingsWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"InputCore",
				"Engine",
				"Slate",
				"SlateCore",
				
				"RHI",
				"SourceControl",
				"TargetPlatform",
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ExternalImagePicker",
			}
		);
	}
}
