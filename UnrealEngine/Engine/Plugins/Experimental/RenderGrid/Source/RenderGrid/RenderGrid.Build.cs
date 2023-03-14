// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RenderGrid : ModuleRules
{
	public RenderGrid(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

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