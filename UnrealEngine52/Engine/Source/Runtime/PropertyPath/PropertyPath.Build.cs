// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PropertyPath : ModuleRules
{
	public PropertyPath(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] 
		{ 
			"Core",
			"CoreUObject" 
		});

		PrivateIncludePaths.AddRange(
			new string[] {
				"Runtime/PropertyPath/Private"
			}
		);
	}
}
