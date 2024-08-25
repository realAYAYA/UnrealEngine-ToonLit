// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;


public class SblSlate : ModuleRules
{
	public SblSlate(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDefinitions.Add("SBL_SLATE=1");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"SblCore",
				"SlateCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"OutputLog",
				"Slate",
				"SlateReflector",
				"StandaloneRenderer",
				"ToolMenus",
			}
		);
	}
}
