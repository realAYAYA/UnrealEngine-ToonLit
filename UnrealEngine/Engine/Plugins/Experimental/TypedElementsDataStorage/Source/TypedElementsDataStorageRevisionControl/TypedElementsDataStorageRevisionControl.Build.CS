// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TypedElementsDataStorageRevisionControl : ModuleRules
{
	public TypedElementsDataStorageRevisionControl(ReadOnlyTargetRules Target) : base(Target)
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
					"Slate",
					"SlateCore",
					"SourceControl",
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
			
			ShortName = "TElmntsDataStorRevCtrl";
		}
	}
}