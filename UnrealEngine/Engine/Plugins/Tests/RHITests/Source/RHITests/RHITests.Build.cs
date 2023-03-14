// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RHITests : ModuleRules
{
	public RHITests(ReadOnlyTargetRules Target) : base(Target)
	{				
		PrivateIncludePaths.AddRange(
			new string[] {
				"../../../../Source/Runtime/Engine/",
				"../../../../Source/Runtime/RHI/",
				// ... add other private include paths required here ...
			}
		);
			
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
                "Engine",
                "RHI",
                "Engine",
                "RenderCore",
				// ... add other public dependencies that you statically link with here ...
			}
		);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Projects",
				// ... add private dependencies that you statically link with here ...	
			}
		);
	}
}
