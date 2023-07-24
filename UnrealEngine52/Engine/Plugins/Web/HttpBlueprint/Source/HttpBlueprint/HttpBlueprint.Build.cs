// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class HttpBlueprint : ModuleRules
{
	public HttpBlueprint(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"HTTP",
				"Slate",
				"SlateCore",
			}
		);
	}
}
