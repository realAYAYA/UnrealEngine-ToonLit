// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PointCloud : ModuleRules
	{
		public PointCloud(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "Engine",
					"RenderCore",
					"RHI",
                    "AugmentedReality",
                    // ... add other public dependencies that you statically link with here ...
                }
                );

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
    				"CoreUObject"
					// ... add private dependencies that you statically link with here ...
				}
				);

			DynamicallyLoadedModuleNames.AddRange(
				new string[]
				{
					// ... add any modules that your module loads dynamically here ...
				}
				);
		}
	}
}
