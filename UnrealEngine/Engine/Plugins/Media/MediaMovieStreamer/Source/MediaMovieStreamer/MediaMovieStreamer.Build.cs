// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MediaMovieStreamer : ModuleRules
{
	public MediaMovieStreamer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Media",
				"MediaAssets",
				"MoviePlayer",
				"RenderCore",
				"RHI",
				"Slate",
				"SlateCore",
			}
			);
		
	}
}
