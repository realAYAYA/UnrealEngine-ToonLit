// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MassEntityEditor : ModuleRules
	{
		public MassEntityEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
			//BinariesSubFolder = "NotForLicensees";

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
