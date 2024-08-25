// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AIGraph : ModuleRules
{
	public AIGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		OverridePackageType = PackageOverrideType.EngineDeveloper;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AIModule",
				"ApplicationCore",
				"BlueprintGraph",
				"Core",
				"GraphEditor",
				"InputCore",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
			}
		);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
			}
		);
	}
}