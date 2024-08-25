// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PhysicsCore: ModuleRules
{
	public PhysicsCore(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject"
			}
		);

		PublicDependencyModuleNames.AddRange(
		   new string[] {
				"DeveloperSettings"
		   }
	   );

		SetupModulePhysicsSupport(Target);
		

		// SetupModulePhysicsSupport adds a dependency on PhysicsCore, but we are PhysicsCore!
		PublicIncludePathModuleNames.Remove("PhysicsCore");
		PublicDependencyModuleNames.Remove("PhysicsCore");

		PrivateDefinitions.Add("CHAOS_INCLUDE_LEVEL_1=1");

		bAllowAutoRTFMInstrumentation = true;
	}
}
