// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GLTFMaterialBaking : ModuleRules
{
	public GLTFMaterialBaking(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string [] {
				"RenderCore",
				"RHI",
				"UnrealEd",
				"MeshDescription",
				"StaticMeshDescription",
			}
		);

		// NOTE: ugly hack to access HLSLMaterialTranslator to bake shading model
		PrivateIncludePaths.Add(EngineDirectory + "/Source/Runtime/Engine/Private");
	}
}
