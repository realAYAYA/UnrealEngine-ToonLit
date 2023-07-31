// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class CADKernelSurface : ModuleRules
	{
		public CADKernelSurface(ReadOnlyTargetRules Target) : base(Target)
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
					"CADKernel",
					"CADLibrary",
					"CADTools",
					"CoreUObject",
					"DatasmithContent",
					"DatasmithCore",
					"DatasmithTranslator",
					"Engine",
					"MeshDescription",
					"ParametricSurface",
					"StaticMeshDescription",
				}
			);
		}
	}
}
