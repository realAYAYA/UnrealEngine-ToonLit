// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class LearningAgentsTraining : ModuleRules
{
	public LearningAgentsTraining(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			});
		
		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			});
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
			});
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Learning",
				"LearningAgents",
				"LearningTraining",
				// ... add private dependencies that you statically link with here ...	
			});

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.Add("UnrealEd");
		}

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			});
	}
}
