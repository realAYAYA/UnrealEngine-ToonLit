// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GeometryCollectionEngine : ModuleRules
	{
        public GeometryCollectionEngine(ReadOnlyTargetRules Target) : base(Target)
		{
			SetupModulePhysicsSupport(Target);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
                    "Engine",
                    "RenderCore",
                    "RHI",
					"Renderer",
                    "FieldSystemEngine",
	                "ChaosSolverEngine",
					"NetCore",
					"IntelISPC",
					"DataflowCore",
					"DataflowEngine",
					"MeshDescription",
					"StaticMeshDescription",
				}
				);
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"MeshConversion",
					"GeometryCore",
				}
				);

			PrivateIncludePathModuleNames.Add("DerivedDataCache");

			if (Target.bBuildEditor)
            {
				DynamicallyLoadedModuleNames.Add("NaniteBuilder");
				PrivateIncludePathModuleNames.Add("NaniteBuilder");

				PublicDependencyModuleNames.Add("EditorFramework");
                PublicDependencyModuleNames.Add("UnrealEd");
			}

			PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
		}
	}
}
