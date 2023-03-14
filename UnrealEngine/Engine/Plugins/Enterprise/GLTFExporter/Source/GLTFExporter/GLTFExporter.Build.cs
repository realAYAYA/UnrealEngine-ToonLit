// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GLTFExporter : ModuleRules
{
	public GLTFExporter(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Json",
				"RenderCore",
				"RHI",
				"ImageWrapper",
				"LevelSequence",
				"MovieScene",
				"MovieSceneTracks",
				"VariantManagerContent",
				"Projects",
				"Analytics",
			}
		);

		if (Target.bBuildEditor)
		{
			// TODO: remove this when we no longer need to include GLTFMaterialBakingStructures.h in GLTFMeshData.h
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"GLTFMaterialBaking",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"MessageLog",
					"Slate",
					"SlateCore",
					"MainFrame",
					"InputCore",
					"EditorStyle",
					"PropertyEditor",
					"ToolMenus",
					"MaterialUtilities",
					"MeshMergeUtilities",
					"MeshDescription",
					"StaticMeshDescription",
					"GLTFMaterialAnalyzer",
				}
			);

			PrivateIncludePaths.AddRange(new string[] {
				System.IO.Path.Combine(GetModuleDirectory("MeshMergeUtilities"), "Private"),
			});
		}
	}
}
