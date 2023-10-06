// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EditorPerformanceModule : ModuleRules
{
	public EditorPerformanceModule(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"CoreUObject",
				"Engine",
				"EditorSubsystem",
				"InputCore",
				"MessageLog",
				"SlateCore",
				"Slate",
				"ToolMenus",
				"WorkspaceMenuStructure"
			}
		);
	}
}