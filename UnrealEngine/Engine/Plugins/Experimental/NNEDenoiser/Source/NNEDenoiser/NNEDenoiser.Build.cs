// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNEDenoiser : ModuleRules
{
	public NNEDenoiser(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"RenderCore"
			});
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"NNE",
				"NNEDenoiserShaders",
				"Renderer",
				"RHI"
			});
	}
}
