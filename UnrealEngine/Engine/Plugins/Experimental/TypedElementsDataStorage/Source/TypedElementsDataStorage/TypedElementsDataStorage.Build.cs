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
					"EditorSubsystem",
					"MassActors",
					"MassEntity",
					"MassEntityEditor",
					"MassGameplayEditor",
					"MassSimulation",
					"TypedElementFramework",
					"SlateCore",
					"StructUtils",
					"UnrealEd"
				});

			PrivateDependencyModuleNames.AddRange(new string[] {});
			DynamicallyLoadedModuleNames.AddRange(new string[] {});
		}

		ShortName = "TElmntsDataStor";
	}
}
