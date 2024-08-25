// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TedsOutliner : ModuleRules
{
	public TedsOutliner(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		if (Target.bBuildEditor)
		{
			PublicIncludePaths.AddRange(new string[] {});
			PrivateIncludePaths.AddRange(new string[] {});

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"EditorFramework",
					"Engine",
					"MassActors",
					"MassEntity",
					"SceneOutliner",
					"Slate",
					"SlateCore",
					"StructUtils",
					"TypedElementFramework",
				});
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
				});

			DynamicallyLoadedModuleNames.AddRange(new string[] {});
		}
	}
}
