// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ChaosVD : ModuleRules
{
	public ChaosVD(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RenderCore",
				"ChaosVDRuntime", 
				"TraceServices",
				"ChaosVDData",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DesktopPlatform",
				"Projects",
				"InputCore",
				"EditorFramework",
				"UnrealEd",
				"ToolMenus",
				"Slate",
				"SlateCore", 
				"OutputLog",
				"WorkspaceMenuStructure", 
				"TraceAnalysis", 
				"TraceInsights",
				"TraceLog", 
				"GeometryFramework",
				"GeometryCore",
				"MeshConversion",
				"MeshDescription",
				"StaticMeshDescription", 
				"DynamicMesh",
				"SceneOutliner",
				"TypedElementRuntime",
				"TypedElementFramework"
			}
			);

		SetupModulePhysicsSupport(Target);
	}
}
