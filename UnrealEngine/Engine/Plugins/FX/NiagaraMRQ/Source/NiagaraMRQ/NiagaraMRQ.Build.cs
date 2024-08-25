// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NiagaraMRQ : ModuleRules
	{
		public NiagaraMRQ(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(new[] { "Core" });

			PrivateDependencyModuleNames.AddRange(
				new[]
				{
					"CoreUObject",
					"Engine",
					"Slate",
					"SlateCore",
					"Projects",
					"MovieRenderPipelineCore",
				
					// Data interface dependencies
					"Niagara",
					"NiagaraCore",
					"VectorVM",
					"RenderCore",
					"RHI"
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}
		}
	}
}
