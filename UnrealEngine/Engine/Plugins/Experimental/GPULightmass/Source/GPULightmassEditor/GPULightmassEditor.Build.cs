// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GPULightmassEditor : ModuleRules
	{
		public GPULightmassEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"Landscape",
					"RenderCore",
					"Renderer",
					"RHI",
					"Projects",
					"UnrealEd",
					"SlateCore",
					"Slate",
					"GPULightmass",
					"WorkspaceMenuStructure",
					"EditorStyle",
					"ToolMenus",
				}
				);
		}
	}
}
