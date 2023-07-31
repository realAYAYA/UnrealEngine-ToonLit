// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DMXFixtures : ModuleRules
{
	public DMXFixtures(ReadOnlyTargetRules Target) : base(Target)
	{
	   PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

       PublicDependencyModuleNames.AddRange(new string[] { 
		   "Core",
		   "CoreUObject",
			"DMXFixtureActorInterface",
		   "DMXRuntime", 
		   "DMXProtocol",
		   "Engine",
		   "RenderCore", 
		   "RHI", 
		   "ProceduralMeshComponent" });

       PrivateDependencyModuleNames.AddRange(new string[] {  });

	}
}
