// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MemoryUsageQueries : ModuleRules
	{
		public MemoryUsageQueries(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
			bAllowConfidentialPlatformDefines = true;

			PublicIncludePaths.AddRange(
				new string[] {
					"$(ModuleDir)/Public",
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetRegistry",
					"Engine",
					"EngineSettings",
					"PakFile"
				}
			);
		}
	}
}