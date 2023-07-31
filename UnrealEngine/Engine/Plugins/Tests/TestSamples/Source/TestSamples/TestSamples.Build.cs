// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TestSamples : ModuleRules
{
	public TestSamples(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
