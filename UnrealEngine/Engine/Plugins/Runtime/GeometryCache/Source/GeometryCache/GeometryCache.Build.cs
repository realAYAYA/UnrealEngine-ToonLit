// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryCache : ModuleRules
{
	public GeometryCache(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("../../../../Shaders/Shared");

        PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "InputCore",
                "RenderCore",
                "RHI",
                "Niagara",
                "NiagaraCore"
			}
		);

        PublicIncludePathModuleNames.Add("TargetPlatform");

        if (Target.bBuildEditor)
        {
            PublicIncludePathModuleNames.Add("GeometryCacheEd");
            DynamicallyLoadedModuleNames.Add("GeometryCacheEd");
            PrivateDependencyModuleNames.Add("MeshUtilitiesCommon");
            PrivateDependencyModuleNames.Add("UnrealEd");
        }        
	}
}
