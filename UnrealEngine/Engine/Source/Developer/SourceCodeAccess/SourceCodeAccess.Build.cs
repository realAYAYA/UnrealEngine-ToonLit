// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SourceCodeAccess : ModuleRules
{
	public SourceCodeAccess(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Settings",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
				"Slate",
				"SlateCore",
			}
		);
	}
}
