// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class QuadricMeshReduction : ModuleRules
{
	public QuadricMeshReduction(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"NaniteUtilities",
				"MeshUtilitiesCommon",
				"MeshDescription",
				"StaticMeshDescription",
			}
		);

        PrivateIncludePathModuleNames.AddRange(
        new string[] {
                "MeshReductionInterface",
             }
        );

        PublicIncludePaths.Add("Developer/MeshSimplifier/Private");
        //PrivateIncludePaths.Add("Developer/MeshSimplifier/Private");
    }
}
