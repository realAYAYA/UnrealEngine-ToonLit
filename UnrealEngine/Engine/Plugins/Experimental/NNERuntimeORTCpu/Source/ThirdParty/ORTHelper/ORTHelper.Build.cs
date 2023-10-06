// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ORTHelper : ModuleRules
{
	public ORTHelper( ReadOnlyTargetRules Target ) : base( Target )
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, "..")
			}
		);

		PublicDependencyModuleNames.AddRange
			(
			new string[] {
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				"Projects"
			}
		);
	}
}
