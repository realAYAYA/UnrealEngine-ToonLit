// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class VariantManagerContentEditor : ModuleRules
	{
		public VariantManagerContentEditor(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetDefinition",
					"ContentBrowser",
					"Core",
					"CoreUObject",
					"EditorFramework",
					"Engine",
					"ToolMenus",
					"UnrealEd",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"InputCore", // For ListView keyboard control
					"Slate",
					"SlateCore",
					"VariantManagerContent",
					"WorkspaceMenuStructure",
				}
			);
		}
	}
}
