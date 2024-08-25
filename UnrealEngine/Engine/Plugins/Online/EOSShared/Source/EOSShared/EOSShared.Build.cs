// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.IO;
using UnrealBuildTool;

public class EOSShared : ModuleRules
{
	[ConfigFile(ConfigHierarchyType.Engine, "EOSShared")]
	bool bEnableApiVersionWarnings = true;

	bool EnableApiVersionWarnings
	{
		get
		{
			ConfigCache.ReadSettings(DirectoryReference.FromFile(Target.ProjectFile), Target.Platform, this);
			return bEnableApiVersionWarnings;
		}
	}

	public EOSShared(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;

		PublicDefinitions.Add("UE_WITH_EOS_SDK_APIVERSION_WARNINGS=" + (EnableApiVersionWarnings ? "1" : "0"));

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"EOSSDK"
			}
		);

		if (Target.bCompileAgainstEngine)
		{
			PrivateDependencyModuleNames.Add("Slate");
		}
	}
}
