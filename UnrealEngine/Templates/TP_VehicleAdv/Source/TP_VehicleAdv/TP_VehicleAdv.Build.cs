// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TP_VehicleAdv : ModuleRules
{
	public TP_VehicleAdv(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "ChaosVehicles", "HeadMountedDisplay", "PhysicsCore" });

		PublicDefinitions.Add("HMD_MODULE_INCLUDED=1");
	}
}
