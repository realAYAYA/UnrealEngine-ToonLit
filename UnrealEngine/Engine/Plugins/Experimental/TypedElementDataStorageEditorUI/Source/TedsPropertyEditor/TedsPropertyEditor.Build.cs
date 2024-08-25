// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TedsPropertyEditor : ModuleRules
{
	public TedsPropertyEditor(ReadOnlyTargetRules Target) : base(Target)
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
					"SceneOutliner",
					"Slate",
					"SlateCore",
					"StructUtils",
					"TypedElementFramework",
					"TedsOutliner"
				});
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd" // For EditorUndoClient
				});

			DynamicallyLoadedModuleNames.AddRange(new string[] {});
		}
	}
}
