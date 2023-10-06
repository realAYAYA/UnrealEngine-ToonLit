// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class FieldNotification: ModuleRules
{
	public FieldNotification(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
			}
		);
	}
}
