// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DisplayClusterScenePreview : ModuleRules
{
	public DisplayClusterScenePreview(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"DisplayClusterLightCardEditorShaders",
				"DisplayClusterLightCardExtender",
				"RHI"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"DisplayCluster",
				"DisplayClusterConfiguration",

				"Core",
				"CoreUObject",
				"Engine",
				"ProceduralMeshComponent",
				"RenderCore"
			});

		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}
	}
}
