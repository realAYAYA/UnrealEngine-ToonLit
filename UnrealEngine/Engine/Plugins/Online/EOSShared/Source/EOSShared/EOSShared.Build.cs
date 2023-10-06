// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.IO;
using UnrealBuildTool;

public class EOSShared : ModuleRules
{
	bool bEnableApiVersionWarnings
	{
		get
		{
			bool bResult;
			ConfigHierarchy Config = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(Target.ProjectFile), Target.Platform);
			if(!Config.GetBool("EOSShared", "bEnableApiVersionWarnings", out bResult))
			{
				// Default to true
				bResult = true;
			}
			return bResult;
		}
	}

	public EOSShared(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;

		PublicDefinitions.Add("UE_WITH_EOS_SDK_APIVERSION_WARNINGS=" + (bEnableApiVersionWarnings ? "1" : "0"));

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"EOSSDK"
			}
		);
	}
}
