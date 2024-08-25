// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class TypedElementFramework : ModuleRules
{
	public TypedElementFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddAll(
			"SlateCore"
		);
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			}
		);
    }
}
