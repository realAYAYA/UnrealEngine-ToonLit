// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BaseTextureBuildWorker : TextureBuildWorker
{
	public BaseTextureBuildWorker(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"TextureBuild",
			"TextureBuildUtilities",
			"TextureFormat",
			"TextureFormatASTC",
			"TextureFormatDXT",
			"TextureFormatETC2",
			"TextureFormatIntelISPCTexComp",
			"TextureFormatUncompressed",
		});
	}
}
