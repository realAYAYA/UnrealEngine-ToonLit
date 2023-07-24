// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class UnrealVirtualizationTool : ModuleRules
{
	public UnrealVirtualizationTool(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Virtualization",
				"Projects",
				"ApplicationCore",
				"SourceControl",
				"PerforceSourceControl",
				"DesktopPlatform"
			});

		PublicIncludePaths.Add("Runtime/Launch/Public");
		PrivateIncludePaths.Add("Runtime/Launch/Private");      // For LaunchEngineLoop.cpp include
	}
}
