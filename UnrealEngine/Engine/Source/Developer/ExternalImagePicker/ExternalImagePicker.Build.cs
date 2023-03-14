// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ExternalImagePicker : ModuleRules
{
	public ExternalImagePicker(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
				"Core",
				"Slate",
				"SlateCore",
				"DesktopPlatform",
				"ImageWrapper",
				"ImageCore",

				"InputCore",
				"PropertyEditor",	// for 'reset to default'
            }
        );
	}
}
