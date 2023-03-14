// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WarpUtils : ModuleRules
{
	public WarpUtils(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			});
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
			});
	}
}
