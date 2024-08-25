// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class EpicWebHelper : ModuleRules
{
	public EpicWebHelper(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

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

