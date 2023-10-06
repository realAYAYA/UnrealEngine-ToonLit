// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RenderGridDeveloper : ModuleRules
{
	public RenderGridDeveloper(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		//PCHUsage = PCHUsageMode.NoPCHs;
		//bUseUnity = false;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Blutility",
				"Core",
				"CoreUObject",
				"Engine",
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