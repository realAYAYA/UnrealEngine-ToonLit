// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AvalancheShapes : ModuleRules
{
	public AvalancheShapes(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Avalanche",
				"AvalancheInteractiveToolsRuntime",
				"Core",
				"CoreUObject",
				"Engine",
				"StructUtils"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ActorModifierCore",
				"DynamicMaterial",
				"DynamicMesh",
				"GeometryCore",
				"GeometryFramework",
				"GeometryScriptingCore",
				"GeometryAlgorithms",
				"InputCore",
				"MovieScene",
				"MovieSceneTracks"
			}
		);

		if (Target.Type == TargetRules.TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"AvalancheEditorCore",
				"DynamicMaterialEditor",
				"TypedElementFramework",
				"UnrealEd",
			});
		}
	}
}
