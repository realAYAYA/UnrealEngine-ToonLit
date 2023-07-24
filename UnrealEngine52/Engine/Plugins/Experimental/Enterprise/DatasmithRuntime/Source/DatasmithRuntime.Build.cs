// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class DatasmithRuntime : ModuleRules
{
	public DatasmithRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		OptimizeCode = CodeOptimization.InShippingBuildsOnly;
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add("Private");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"DatasmithContent",
				"DatasmithCore",
				"DatasmithNativeTranslator",
				"DatasmithGLTFTranslator",
				"DatasmithTranslator",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CinematicCamera",
				"Core",
				"CoreUObject",
				"DirectLink",
				"Engine",
				"IESFile",
				"FreeImage",
				"Landscape",
				"LevelSequence",
				"MeshDescription",
				"MeshUtilitiesCommon",
				"RawMesh",
				"RHI",
				"PhysicsCore",
				"RenderCore",
				"SlateCore",
				"StaticMeshDescription",
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"DesktopPlatform",
					"DeveloperToolSettings",
					"MessageLog",
                    "UnrealEd",
                }
			);
		}

		// Set environment variable DIRECTLINK_LOG to get DirectLink logging
		string DirectLog = System.Environment.GetEnvironmentVariable("DIRECTLINK_LOG");
		if (DirectLog != null)
		{
			PublicDefinitions.Add("DIRECTLINK_LOG");
		}
	}
}