// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.IO;
using UnrealBuildTool;

public class EOSOverlayInputProvider : ModuleRules
{
	public EOSOverlayInputProvider(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;

		UnsafeTypeCastWarningLevel = WarningLevel.Error;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",				
				"EOSSDK",
				"EOSShared",
				"InputCore",
				"SlateCore",
				"Slate"
			}
		);
	}
}
