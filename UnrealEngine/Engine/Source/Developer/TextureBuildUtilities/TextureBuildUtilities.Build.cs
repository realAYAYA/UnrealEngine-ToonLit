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
			"TextureCompressor",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",			
				"ImageCore",
				"TextureFormat",
			}
			);

	}
}
