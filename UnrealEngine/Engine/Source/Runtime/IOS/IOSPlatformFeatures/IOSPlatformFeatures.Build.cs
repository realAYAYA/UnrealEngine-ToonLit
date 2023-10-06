// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IOSPlatformFeatures : ModuleRules
{
	public IOSPlatformFeatures(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] 
			{ 
				"Core", 
				"Engine" 
			});
	}
}
