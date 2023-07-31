// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WinInet : ModuleRules
{
	public WinInet(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemLibraries.Add("wininet.lib");
		}
	}
}

