// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DMXPixelMappingEditorWidgets : ModuleRules
{
	public DMXPixelMappingEditorWidgets(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange( new string[] {
			"Core",
			"CoreUObject",
			"Slate",
			"SlateCore",
		});

		PrivateDependencyModuleNames.AddRange( new string[] {
			"Core",
			"Slate",
			"SlateCore",
			
			"DMXProtocol",
			"DMXRuntime",
			"DMXPixelMappingCore",
		});
	}
}
