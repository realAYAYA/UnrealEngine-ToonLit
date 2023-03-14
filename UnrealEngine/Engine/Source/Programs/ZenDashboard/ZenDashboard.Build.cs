// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ZenDashboard : ModuleRules
{
	public ZenDashboard(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Json",
				"Projects",
				"ApplicationCore",
				"Slate",
				"SlateCore",
				"StandaloneRenderer",
				"Zen",
				
			});

		PublicIncludePaths.Add("Runtime/Launch/Public");
		PrivateIncludePaths.Add("Runtime/Launch/Private");      // For LaunchEngineLoop.cpp include
	}
}
