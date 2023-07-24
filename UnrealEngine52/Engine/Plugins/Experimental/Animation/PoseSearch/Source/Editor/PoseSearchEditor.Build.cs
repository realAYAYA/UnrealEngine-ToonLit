// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
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
				"StructUtils",
				
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

		// TODO: Should not be including private headers from a different module
		PrivateIncludePaths.AddRange(
			new string[] {
				Path.Combine(GetModuleDirectory("PoseSearch"), "Private"), // For PoseSearchTraceLogger.h
			});
	}
}