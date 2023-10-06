// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ParametricSurfaceExtension : ModuleRules
	{
		public ParametricSurfaceExtension(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"MeshDescription",
					"DataprepCore",
					"DatasmithContent",
					"ParametricSurface",
					"PhysicsCore",
					"DatasmithImporter",
					"Engine",
					"StaticMeshEditor",
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
					"EditorFramework",
					"UnrealEd",
					"CADLibrary",
				}
			);
		}
	}
}
