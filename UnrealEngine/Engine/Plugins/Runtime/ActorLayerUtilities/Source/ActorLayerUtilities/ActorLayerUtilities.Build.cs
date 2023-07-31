// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ActorLayerUtilities : ModuleRules
{
	public ActorLayerUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Engine"
			}
		);

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject"
			}
		);
	}
}
