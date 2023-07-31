// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TraceUtilities : ModuleRules
{
	public TraceUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"TraceLog",
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
