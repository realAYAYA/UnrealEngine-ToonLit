// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class IKRig : ModuleRules
{
	public IKRig(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"AnimationCore",
				"PBIK",
			});

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"ControlRig",
				"RigVM",
				"Core",
				"PBIK"
			});

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			});
		
		if (Target.bBuildEditor == true)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
				});
		}
	}
}
