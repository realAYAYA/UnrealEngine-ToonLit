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
					"Core",
					"CoreUObject",
				    "Slate",
				    "SlateCore",
				    "Engine",
					"EditorFramework",
					"UnrealEd",
					"Projects",
					"PropertyEditor",
				    "RenderCore",
				    "RHI",
                    "ChaosFleshEngine",
					"ChaosFlesh",
					"RawMesh",
				    "AssetTools",
				    "AssetRegistry",
				    "SceneOutliner",
					"EditorStyle",
					"ToolMenus",
					"Chaos",
					"MeshDescription",
					"StaticMeshDescription",
					"LevelEditor",
					"InputCore",
					"AdvancedPreviewScene",
					"GraphEditor",
					"DataflowCore",
					"DataflowEngine",
					"DataflowEnginePlugin",
					"DataflowEditor",
					"SkeletonEditor",
				}
			);

			PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
		}
	}
}
