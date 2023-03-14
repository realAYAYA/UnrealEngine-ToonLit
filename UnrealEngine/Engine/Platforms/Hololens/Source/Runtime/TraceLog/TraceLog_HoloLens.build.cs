// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class TraceLog_HoloLens : TraceLog
	{
		public TraceLog_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			EnableTraceByDefault(Target);
		}
	}
}
