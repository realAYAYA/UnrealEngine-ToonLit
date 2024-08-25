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
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"LogVisualizer",
				"TraceAnalysis",
				"TraceServices",
				"TraceInsights",
				"RewindDebuggerInterface",
				"GameplayInsights",
				"Slate",
				"SlateCore",
				"ToolWidgets",
				"ToolMenus",
				"DeveloperSettings",
			});
		}
	}
}

