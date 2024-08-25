// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class TextureGraphInsight : ModuleRules
{
	public TextureGraphInsight(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bUseRTTI = true;
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"RenderCore",
			"RHI"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"TextureGraphEngine",
			"FreeImage",
			"Function2",
			"Continuable",
		});
	}
}
