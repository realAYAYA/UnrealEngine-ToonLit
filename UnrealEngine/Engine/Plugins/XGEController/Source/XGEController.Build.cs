// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class XGEController : ModuleRules
{
	public XGEController(ReadOnlyTargetRules TargetRules)
		: base(TargetRules)
	{
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",

			// all these below are only to get access to GShaderCompilingManager
			"Slate",
			"RHI",
			"RenderCore",
			"Engine",
		});

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"DistributedBuildInterface",
			}
		);
	}
}
