// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class PhysicsUtilities : ModuleRules
{
	public PhysicsUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicDependencyModuleNames.AddRange(
            new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"PhysicsCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"MeshUtilitiesCommon",
				"MeshUtilitiesEngine",
            }
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "VHACD");
	}
}
