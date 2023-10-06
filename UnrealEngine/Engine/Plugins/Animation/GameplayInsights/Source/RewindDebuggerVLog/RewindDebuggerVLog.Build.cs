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
				"LogVisualizer",
				"TraceAnalysis",
				"TraceServices",
				"TraceInsights",
				"RewindDebuggerInterface"
			});
		}
	}
}

