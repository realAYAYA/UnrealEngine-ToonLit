// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryFramework : ModuleRules
{	
	public GeometryFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		//PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// this is required for raytracing suppport?
		PublicIncludePaths.Add("../Shaders/Shared");

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"RHI",
				"PhysicsCore",
				"InteractiveToolsFramework",
				"MeshDescription",
				"StaticMeshDescription",
				"SkeletalMeshDescription",
				"GeometryCore",
				"MeshConversion"
			}
		);
	}
}
