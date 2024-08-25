// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChaosFleshEditor : ModuleRules
	{
        public ChaosFleshEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"AdvancedPreviewScene",
					"AssetDefinition",
					"AssetRegistry",
					"AssetTools",
					"Chaos",
					"ChaosFlesh",
					"ChaosFleshEngine",
					"Core",
					"CoreUObject",
					"DataflowCore",
					"DataflowEditor",
					"DataflowEngine",
					"DataflowEnginePlugin",
					"Engine",
					"EditorFramework",
					"EditorStyle",
					"GraphEditor",
					"InputCore",
					"LevelEditor",
					"MeshDescription",
					"Slate",
				    "SlateCore",
					"Projects",
					"PropertyEditor",
					"RHI",
					"RawMesh",
					"RenderCore",
					"SceneOutliner",
					"SkeletonEditor",
					"StaticMeshDescription",
					"ToolMenus",
					"UnrealEd",
				}
			);

			PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
		}
	}
}
