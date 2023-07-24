// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StructUtilsEditor : ModuleRules
	{
		public StructUtilsEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"AssetTools",
				"UnrealEd",
				"Slate",
				"SlateCore",				
				"PropertyEditor",
				"AIModule",
				"StructUtils",
				"DetailCustomizations",
				"ComponentVisualizers",
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"RenderCore",
				"GraphEditor",
				"KismetWidgets",
				"PropertyEditor",
				"AIGraph",
				"ToolMenus",
				"BlueprintGraph",
			}
			);
		}

	}
}
