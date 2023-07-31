// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class SynthBenchmark : ModuleRules
	{
		public SynthBenchmark(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicIncludePathModuleNames.Add("Renderer");

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
        			"RenderCore", 
				    "RHI",
					// ... add other public dependencies that you statically link with here ...
				}
				);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore"
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
