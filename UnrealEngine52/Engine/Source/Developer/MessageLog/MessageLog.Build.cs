// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class MessageLog : ModuleRules
{
	public MessageLog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"ApplicationCore",
				"Slate",
				"SlateCore",
                "InputCore",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"Documentation",
					"MainFrame",
					"WorkspaceMenuStructure"
				}
			);		
		}

		if (Target.bCompileAgainstEngine && Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrecompileForTargets = PrecompileTargetsType.Any;
		}
	}
}
