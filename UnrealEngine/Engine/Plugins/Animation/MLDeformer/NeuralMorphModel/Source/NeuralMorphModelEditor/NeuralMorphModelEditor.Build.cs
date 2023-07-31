// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class NeuralMorphModelEditor : ModuleRules
	{
		public NeuralMorphModelEditor(ReadOnlyTargetRules Target) : base(Target)
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
					"NeuralMorphModel",
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
