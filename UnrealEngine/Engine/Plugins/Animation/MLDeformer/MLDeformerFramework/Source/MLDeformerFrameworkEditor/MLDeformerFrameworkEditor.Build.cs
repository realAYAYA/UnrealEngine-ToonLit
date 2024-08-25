// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class MLDeformerFrameworkEditor : ModuleRules
	{
		public MLDeformerFrameworkEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.AddRange(
				new string[] 
				{
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"SequencerWidgets"
				}
			);

			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"GeometryCache",
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
					"TimeManagement",
					"RenderCore",
					"RHI",
					"AssetDefinition",
					"SkeletalMeshDescription",
				}
			);

			PublicIncludePathModuleNames.AddRange(
				new string[]
				{
					"Persona",
				}
			);
		}
	}
}
