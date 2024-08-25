// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class GeometryMaskEditor : ModuleRules
{
	public GeometryMaskEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[] {
				"CoreUObject",
                "GeometryMask",
                "SlateCore",
            });

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"Engine",
				"InputCore",
				"Slate",
				"UnrealEd",
				"WorkspaceMenuStructure",
			});
    }
}
