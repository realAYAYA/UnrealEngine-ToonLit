// Copyright Epic Games, Inc. All Rights Reserved.
using EpicGames.Core;
using System;
using System.IO;
using UnrealBuildTool;

public class XInput : ModuleRules
{

	public XInput(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
		{
			PublicSystemLibraries.Add("XInput.lib");
			PublicDelayLoadDLLs.Add("XInput1_4.dll");
		}
	}
}

