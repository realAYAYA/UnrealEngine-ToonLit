// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class HairStrandsDeformer : ModuleRules
	{
		public HairStrandsDeformer(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"NiagaraCore",
					"Niagara",
					"RenderCore",
					"Renderer",
					"RHI",
					"ComputeFramework",
					"OptimusCore",
					"HairStrandsCore"
				});

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"Shaders",
				});
		}
	}
}
