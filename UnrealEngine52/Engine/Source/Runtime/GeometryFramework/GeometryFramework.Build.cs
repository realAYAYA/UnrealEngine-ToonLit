// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryFramework : ModuleRules
{	
	public GeometryFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		//PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// this is required for raytracing suppport?
		PublicIncludePaths.Add("../Shaders/Shared");
		
		// These are required to register filters to the Level Editor Outliner
		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"EditorWidgets",
				}
			);
				
			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{													
					"LevelEditor",
				}
			);
		}

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
