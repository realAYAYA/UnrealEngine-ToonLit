// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InteractiveToolsFramework : ModuleRules
{
	public InteractiveToolsFramework(ReadOnlyTargetRules Target) : base(Target)
	{
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
                "Core",
                "CoreUObject",
                "InputCore",
                "ApplicationCore",
				"MeshDescription"
				// ... add other public dependencies that you statically link with here ...
			}
            );			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Engine",
                "RHI",
				"GeometryCore"
				//"Slate",
				//"SlateCore",
				// ... add private dependencies that you statically link with here ...	
			}
			);
	}
}
