// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureBuildUtilities : ModuleRules
{
	public TextureBuildUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		// TextureBuildUtilities can be used in TBW so cannot depend on Engine

		// Include only , no link :
		PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
			"TextureCompressor"
		});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",			
				"DerivedDataCache",
				"ImageCore",
				"ImageWrapper",
				"TextureFormat",
			}
			);

	}
}
