// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DataprepCore : ModuleRules
	{
		public DataprepCore(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"BlueprintGraph",
					"Core",
					"CoreUObject",
					"EditorFramework",
					"EditorScriptingUtilities",
					"EditorWidgets",
					"Engine",
					"GraphEditor",
					"InputCore",
					"KismetCompiler",
					"LevelSequence",
					"MeshDescription",
					"MessageLog",
					"PropertyEditor",
					"RenderCore",
					"RHI",
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
					"UnrealEd",
					"VariantManagerContent",
				}
			);
		}
	}
}
