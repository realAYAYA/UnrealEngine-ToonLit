// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PCG : ModuleRules
	{
		public PCG(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"Landscape",
					"Foliage",
					"Projects",
					"RenderCore",
					"RHI",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{					
					"Voronoi",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
						"SourceControl",
					}
				);
			}

			PrivateIncludePaths.AddRange(
				new string[] {
				});
		}
	}
}
