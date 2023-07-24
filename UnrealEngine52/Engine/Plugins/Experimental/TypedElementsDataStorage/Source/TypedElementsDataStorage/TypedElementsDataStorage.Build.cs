// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TypedElementsDataStorage : ModuleRules
{
	public TypedElementsDataStorage(ReadOnlyTargetRules Target) : base(Target)
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
					"MassActors",
					"MassEntity",
					"MassEntityEditor",
					"MassGameplayEditor",
					"TypedElementFramework",
					"EditorSubsystem",
					"UnrealEd",
				});

			PrivateDependencyModuleNames.AddRange(new string[] {});
			DynamicallyLoadedModuleNames.AddRange(new string[] {});
		}
	}
}
