// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PCGGeometryScriptInterop : ModuleRules
	{
		public PCGGeometryScriptInterop(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"GeometryScriptingCore",
					"Projects",
					"RenderCore",
					"RHI",
					"PCG"
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"GeometryCore",
					"GeometryFramework"
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
					}
				);
			}
		}
	}
}
