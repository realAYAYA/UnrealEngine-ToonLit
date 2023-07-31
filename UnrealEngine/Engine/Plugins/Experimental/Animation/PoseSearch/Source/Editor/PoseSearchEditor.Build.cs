// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class PoseSearchEditor : ModuleRules
{
	public PoseSearchEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AnimGraph",
				"AnimGraphRuntime",
				"AnimationCore",
				"Core",
				"CoreUObject",
				"Engine",
				"PoseSearch",
				"GameplayTags",
				
				// Trace-related dependencies
				"TraceLog",
				"TraceAnalysis",
				"TraceServices",
				"TraceInsights",
				"GameplayInsights",
				
				// UI 
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"RewindDebuggerInterface",
				"UnrealEd",
				"InputCore"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"BlueprintGraph",
				"PropertyEditor",
				"SlateCore",
				"Slate",
				"EditorStyle",
				"DetailCustomizations",
				"AdvancedPreviewScene",
				"EditorFramework",
				"ToolWidgets"
			}
		);
	}
}