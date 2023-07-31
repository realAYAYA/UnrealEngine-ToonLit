// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class CADLibrary : ModuleRules
{
	public CADLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		bLegalToDistributeObjectCode = true;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CADTools",
				"CADKernel",
				"DatasmithCore",
				"DynamicMesh",
				"MeshConversion",
				"MeshDescription",
				"GeometryCore",
				"StaticMeshDescription"
			}
		);

		// Support for Windows only
		bool bIsPlateformSupported = Target.Platform == UnrealTargetPlatform.Win64;
	}
}
