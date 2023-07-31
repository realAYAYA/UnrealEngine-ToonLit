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
				"Engine",
				"CoreUObject",
				"GameplayTags",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"VPUtilities"
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
