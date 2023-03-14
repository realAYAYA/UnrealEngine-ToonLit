// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HttpBlueprintGraph : ModuleRules
{
	public HttpBlueprintGraph(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetRegistry",
				"BlueprintGraph",
				"Core",
				"CoreUObject",
				"Engine",
				"GraphEditor",
				"InputCore",
				"Kismet",
				"KismetCompiler",
				"KismetWidgets",
				"Slate",
				"SlateCore", 
				"ToolMenus"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"EditorStyle",
					"HttpBlueprint",
					"HTTP",
					"JsonBlueprintUtilities",
					"Projects",
					"PropertyEditor",
					"UnrealEd",
				}
			);
		}
	}
}