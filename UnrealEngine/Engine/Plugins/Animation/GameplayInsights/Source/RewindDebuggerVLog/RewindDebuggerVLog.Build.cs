// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RewindDebuggerVLog : ModuleRules
	{
		public RewindDebuggerVLog(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetRegistry",
				"Core",
				"CoreUObject",
				"EditorFramework",
				"EditorWidgets",
				"Engine",
				"GameplayInsights",
				"GameplayInsightsEditor",
				"LogVisualizer",
				"SlateCore",
				"Slate",
				"ToolMenus",
				"TraceLog",
				"TraceAnalysis",
				"TraceServices",
				"TraceInsights",
				"UnrealEd",
				"RewindDebuggerInterface"
			});
		}
	}
}

