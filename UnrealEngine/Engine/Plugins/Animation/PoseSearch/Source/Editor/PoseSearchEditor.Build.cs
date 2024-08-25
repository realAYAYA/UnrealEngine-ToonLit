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
				"BlendStackEditor",
				"BlendStack",
				"AnimGraph",
				"AnimGraphRuntime",
				"AnimationBlueprintLibrary",
				"AnimationCore",
				"Core",
				"CoreUObject",
				"Engine",
				"PoseSearch",
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
				"ApplicationCore",
				"AnimationEditor",
				"AssetDefinition",
				"BlueprintGraph",
				"EditorStyle",
				"DetailCustomizations",
				"AdvancedPreviewScene",
				"EditorFramework",
				"ToolWidgets"
			}
		);
	}
}
