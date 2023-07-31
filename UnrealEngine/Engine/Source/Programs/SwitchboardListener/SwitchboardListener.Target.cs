// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class SwitchboardListenerTarget : TargetRules
{
	public SwitchboardListenerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Program;
		LinkType = TargetLinkType.Monolithic;
		LaunchModuleName = "SwitchboardListener";

		// This app compiles against Core/CoreUObject, but not the Engine or Editor, so compile out Engine and Editor references from Core/CoreUObject
		bCompileAgainstCoreUObject = true;
		bCompileAgainstEngine = false;
		bBuildWithEditorOnlyData = false;

		// Disable internationalization support, so the exe can be launched outside of the engine directory.
		bCompileICU = false;

		// The listener is meant to be a console application (no window), but on MacOS, to get a proper log console, a full application must be built.
		bIsBuildingConsoleApplication = Target.Platform != UnrealTargetPlatform.Mac;

		GlobalDefinitions.Add("UE_TRACE_ENABLED=1");
	}
}
