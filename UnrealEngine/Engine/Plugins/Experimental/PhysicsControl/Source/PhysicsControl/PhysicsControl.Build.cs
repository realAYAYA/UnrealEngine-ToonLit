// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PhysicsControl : ModuleRules
{
	public PhysicsControl(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimationCore",
				"AnimGraphRuntime",
				"Chaos",
				"ChaosCore",
				"Core",
				"CoreUObject",
				"Engine",
				"PhysicsCore"
			}
		);

		SetupModulePhysicsSupport(Target);
		PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");
	}
}
