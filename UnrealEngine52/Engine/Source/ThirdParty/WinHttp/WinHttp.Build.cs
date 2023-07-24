// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WinHttp : ModuleRules
{
	public WinHttp(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemLibraries.Add("winhttp.lib");
		PublicDefinitions.Add("WITH_WINHTTP=1");
	}
}
