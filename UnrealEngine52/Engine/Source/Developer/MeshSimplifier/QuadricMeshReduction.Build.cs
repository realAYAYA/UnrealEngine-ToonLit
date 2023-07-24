// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class QuadricMeshReduction : ModuleRules
{
	public QuadricMeshReduction(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("Engine");
		PrivateDependencyModuleNames.Add("RenderCore");
        PrivateDependencyModuleNames.Add("MeshDescription");
        PrivateDependencyModuleNames.Add("MeshUtilitiesCommon");
        PrivateDependencyModuleNames.Add("StaticMeshDescription");

        PrivateIncludePathModuleNames.AddRange(
        new string[] {
                "MeshReductionInterface",
             }
        );

        PublicIncludePaths.Add("Developer/MeshSimplifier/Private");
        //PrivateIncludePaths.Add("Developer/MeshSimplifier/Private");
    }
}
