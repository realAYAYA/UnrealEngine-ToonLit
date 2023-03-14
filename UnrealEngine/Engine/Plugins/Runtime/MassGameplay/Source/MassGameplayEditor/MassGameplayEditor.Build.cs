// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MassGameplayEditor : ModuleRules
	{
		public MassGameplayEditor(ReadOnlyTargetRules Target) : base(Target)
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
				"MassSpawner",
				"MassEntity",
				"DetailCustomizations",
				"ComponentVisualizers",
				"Projects",
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
			}
			);
		}

	}
}
