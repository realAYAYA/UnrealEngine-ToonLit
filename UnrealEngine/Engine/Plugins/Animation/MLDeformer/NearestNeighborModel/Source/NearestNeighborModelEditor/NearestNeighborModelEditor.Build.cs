// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class NearestNeighborModelEditor : ModuleRules
	{
		public NearestNeighborModelEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorFramework",
					"UnrealEd",
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"EditorWidgets",
					"EditorStyle",
					"GeometryCache",
					"MLDeformerFramework",
					"MLDeformerFrameworkEditor",
					"NearestNeighborModel",
					"Projects",
					"PropertyEditor",
					"NeuralNetworkInference",
					"ToolWidgets",
					"ComputeFramework",
					"RenderCore",
					"RHI"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
				}
			);
		}
	}
}
