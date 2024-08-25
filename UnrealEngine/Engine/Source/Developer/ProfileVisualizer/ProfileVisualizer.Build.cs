// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ProfileVisualizer : ModuleRules
{
	public ProfileVisualizer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Slate",
				"SlateCore",
				
				"Engine",
				"InputCore"
			}
		);

		if ((Target.bCompileAgainstEditor && (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)))
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"WorkspaceMenuStructure",
					"DesktopPlatform"
				}
			);
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrecompileForTargets = PrecompileTargetsType.Any;
		}
	}
}
