// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Mover : ModuleRules
{
	public Mover(ReadOnlyTargetRules Target) : base(Target)
	{		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"NetCore",
				"InputCore",
				"NetworkPrediction",
				"AnimGraphRuntime",
				"MotionWarping",
				"Water",
			}
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Chaos",
				"CoreUObject",
				"Engine",
				"PhysicsCore",
				"DeveloperSettings",
				// ... add private dependencies that you statically link with here ...	
			}
			);

		SetupGameplayDebuggerSupport(Target);
	}
}
