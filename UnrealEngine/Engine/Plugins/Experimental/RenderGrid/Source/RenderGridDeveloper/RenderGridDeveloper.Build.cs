// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RenderGridDeveloper : ModuleRules
{
	public RenderGridDeveloper(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"KismetCompiler",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlueprintGraph",
				"Engine",
				"GraphEditor",
				"RenderGrid",
				"Slate",
				"SlateCore",
				"UnrealEd",
			}
		);
	}
}