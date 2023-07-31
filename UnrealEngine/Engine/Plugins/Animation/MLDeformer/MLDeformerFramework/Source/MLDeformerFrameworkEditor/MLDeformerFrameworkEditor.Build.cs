// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MLDeformerFrameworkEditor : ModuleRules
	{
		public MLDeformerFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"SequencerWidgets"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"InputCore",
					"EditorFramework",
					"UnrealEd",
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"Persona",
					"MLDeformerFramework",
					"Projects",
					"PropertyEditor",
					"AnimationEditMode",
					"AnimGraph",
					"ToolWidgets",
					"EditorWidgets",
					"GeometryCache",
					"SequencerWidgets",
					"TimeManagement",
					"NeuralNetworkInference",
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
