// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DeveloperToolSettings : ModuleRules
{
	public DeveloperToolSettings(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Projects");
		PrivateDependencyModuleNames.Add("CoreUObject");
		PrivateDependencyModuleNames.Add("DeveloperSettings");
		PrivateDependencyModuleNames.Add("DesktopPlatform");
	}
}
