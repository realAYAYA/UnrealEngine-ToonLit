// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MoverEditor : ModuleRules
{
	public MoverEditor(ReadOnlyTargetRules Target) : base(Target)
	{

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"Mover",
				"BlueprintGraph",
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
			});

	}
}