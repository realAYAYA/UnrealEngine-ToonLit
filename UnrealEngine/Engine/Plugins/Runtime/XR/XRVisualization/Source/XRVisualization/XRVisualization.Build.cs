// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class XRVisualization : ModuleRules
	{
		public XRVisualization(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
                    "EngineSettings",
                    "RenderCore",
					"RHI",
					"HeadMountedDisplay",
					"ProceduralMeshComponent",
				}
			);
        }
	}
}
