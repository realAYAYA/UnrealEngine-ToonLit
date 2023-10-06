// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TypedElementsDataStorageUI : ModuleRules
{
	public TypedElementsDataStorageUI(ReadOnlyTargetRules Target) : base(Target)
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
					"Engine",
					"EditorFramework",
					"SceneOutliner",
					"Slate",
					"SlateCore",
					"ToolMenus",
					"TypedElementFramework",
					"TypedElementsDataStorage",
					"EditorSubsystem",
					"UnrealEd",
				});
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"MassActors",
					"MassEntity",
					"StructUtils"
				});

			DynamicallyLoadedModuleNames.AddRange(new string[] {});
		}
	}
}
