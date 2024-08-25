// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class NNEDenoiserShaders : ModuleRules
{
	public NNEDenoiserShaders(ReadOnlyTargetRules Target) : base(Target)
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
				"Projects",
				"Renderer",
				"RHI"
			});
	}
}
