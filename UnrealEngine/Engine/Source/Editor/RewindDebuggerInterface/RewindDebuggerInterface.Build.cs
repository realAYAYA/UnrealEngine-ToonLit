// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RewindDebuggerInterface : ModuleRules
{
	public RewindDebuggerInterface(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"SlateCore",
				"TraceServices"
			}
		);
	}
}
