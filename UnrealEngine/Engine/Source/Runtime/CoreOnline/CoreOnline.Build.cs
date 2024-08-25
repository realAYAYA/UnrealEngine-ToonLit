// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using EpicGames.Core;

public class CoreOnline : ModuleRules
{
	public CoreOnline(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject"
			}
		);

		PublicIncludePathModuleNames.AddRange(
			new string[] {
				"CoreUObject"
			}
		);

		PrivateDefinitions.Add("COREONLINE_PACKAGE=1");
		PublicDefinitions.Add("PLATFORM_MAX_LOCAL_PLAYERS=" + GetPlatformMaxLocalPlayers(Target));

		bAllowAutoRTFMInstrumentation = true;
	}

	protected virtual int GetPlatformMaxLocalPlayers(ReadOnlyTargetRules Target)
	{
		// 0 indicates no platform override
		return 0;
	}
}
