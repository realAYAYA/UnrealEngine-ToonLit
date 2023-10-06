// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SymsLibDump : ModuleRules
{
	public SymsLibDump(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				"Projects",
				"SymsLib",
			}
		);
	}
}
