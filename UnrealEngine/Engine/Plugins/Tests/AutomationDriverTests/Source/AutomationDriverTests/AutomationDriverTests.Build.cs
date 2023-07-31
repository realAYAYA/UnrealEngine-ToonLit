// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AutomationDriverTests : ModuleRules
{
	public AutomationDriverTests(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"InputCore",
				"AutomationDriver",
				
				// ... add other public dependencies that you statically link with here ...
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
				"AutomationDriver",

				"Core",
				"ApplicationCore",
				"Json",

				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
