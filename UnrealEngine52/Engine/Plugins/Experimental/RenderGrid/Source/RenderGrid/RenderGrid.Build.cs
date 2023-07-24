// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RenderGrid : ModuleRules
{
	public RenderGrid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		//PCHUsage = PCHUsageMode.NoPCHs;
		//bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RemoteControl",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json",
				"LevelSequence",
				"LevelSequenceEditor",
				"MovieRenderPipelineCore",
				"MovieRenderPipelineEditor",
				"MovieRenderPipelineRenderPasses",
				"MovieScene",
				"Serialization",
				"UnrealEd",
			}
		);
	}
}