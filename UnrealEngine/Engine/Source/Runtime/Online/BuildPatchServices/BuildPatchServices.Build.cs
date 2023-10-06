// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using UnrealBuildTool;

public class BuildPatchServices : ModuleRules
{
	[ConfigFile(ConfigHierarchyType.Engine, "BuildPatchServices")]
	bool bEnableDiskOverflowStore = true;

	public BuildPatchServices(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject"
			}
		);

		PrivateDependencyModuleNames.AddRange(
		new string[] {
				"Analytics",
				"AnalyticsET",
				"HTTP",
				"Json",
				"VirtualFileCache",
			}
		);

		if (EnableDiskOverflowStore)
		{
			PublicDefinitions.Add("ENABLE_PATCH_DISK_OVERFLOW_STORE=1");
		}
		else
		{
			PublicDefinitions.Add("ENABLE_PATCH_DISK_OVERFLOW_STORE=0");
		}

	}

	protected bool EnableDiskOverflowStore
	{
		get
		{
			ConfigCache.ReadSettings(DirectoryReference.FromFile(Target.ProjectFile), Target.Platform, this);
			return bEnableDiskOverflowStore;
		}
	}
}
