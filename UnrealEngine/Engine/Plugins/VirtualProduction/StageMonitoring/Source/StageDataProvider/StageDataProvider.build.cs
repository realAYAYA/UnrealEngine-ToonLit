// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StageDataProvider : ModuleRules
{
	public StageDataProvider(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"GameplayTags",
				"RenderCore",
				"StageDataCore",
				"StageMonitorCommon",
				"TimeManagement",
				"VPRoles",
				"VPUtilities",
			}
		);

		if (Target.Type == TargetType.Editor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"LevelSequence",
					"TakeRecorder",
				}
			);
		}
	}
}
