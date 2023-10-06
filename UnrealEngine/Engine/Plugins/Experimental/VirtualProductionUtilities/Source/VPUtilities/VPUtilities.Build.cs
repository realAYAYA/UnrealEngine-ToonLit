// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VPUtilities : ModuleRules
{
	public VPUtilities(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"CinematicCamera",
				"Composure",
				"Core",
				"CoreUObject",
				"Engine",
				"GameplayTags",
				"TimeManagement",
				"UMG",
				"VPBookmark",
				"VPSettings"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"Projects",
				"Renderer",
				"RenderCore",
				"RHI",
				"Slate",
				"SlateCore",
				"ConcertClient",
				"VPRoles"
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Blutility",
					"EditorFramework",
					"LevelEditor",
					"UMGEditor",
					"UnrealEd",
					"ViewportInteraction",
					"VPBookmarkEditor",
					"VREditor",
				}
			);
		}
	}
}
