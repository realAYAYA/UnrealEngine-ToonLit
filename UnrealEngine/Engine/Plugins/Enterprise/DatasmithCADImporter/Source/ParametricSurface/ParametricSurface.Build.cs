// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ParametricSurface : ModuleRules
	{
		public ParametricSurface(ReadOnlyTargetRules Target) : base(Target)
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
					"CADInterfaces",
					"CADLibrary",
					"CADTools",
					"DatasmithContent",
					"DatasmithTranslator",
					"Engine",
					"Json",
					"MeshDescription",
					"StaticMeshDescription",
				}
			);
		}
	}
}
