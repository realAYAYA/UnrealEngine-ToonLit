// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateMVVM : ModuleRules
{
	public SlateMVVM(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"FieldNotification",
				"SlateCore",
			}
		);
	}
}
