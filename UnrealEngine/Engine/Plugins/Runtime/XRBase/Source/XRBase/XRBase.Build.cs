// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class XRBase : ModuleRules
	{
		public XRBase(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"HeadMountedDisplay",
					"AugmentedReality",
				}
				);
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
                    "EngineSettings",
					"Renderer",
					"RenderCore",
					"RHI",
					"InputCore",
					"Slate",
				}
			);
			
			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"EditorFramework",
						"UnrealEd",
						"VREditor",
					});
			}
        }
	}
}
