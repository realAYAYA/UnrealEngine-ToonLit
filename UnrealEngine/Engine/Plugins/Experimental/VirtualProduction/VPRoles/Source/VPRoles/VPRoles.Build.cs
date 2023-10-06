// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VPRoles : ModuleRules
{
	public VPRoles(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"VPSettings"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"GameplayTagsEditor",
					"UnrealEd",
				}
			);
		}
	}
}
