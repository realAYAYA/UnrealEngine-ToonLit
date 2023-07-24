// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class EpicWebHelper : ModuleRules
{
	public EpicWebHelper(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.Add("Runtime/Launch/Public");

		PrivateIncludePaths.AddRange(
			new string[]
			{
				"Programs/EpicWebHelper/Private",
				"Runtime/Launch/Private",					// for LaunchEngineLoop.cpp include
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"ApplicationCore",
				"Projects",
				"CEF3Utils",
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target,
			"CEF3"
			);
	}
}

