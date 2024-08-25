// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;


public class SwitchboardListenerCmdTarget : SwitchboardListenerTargetBase
{
	public SwitchboardListenerCmdTarget(TargetInfo Target) : base(Target)
	{
		LaunchModuleName = "SwitchboardListenerCmd";

		// The listener is meant to be a console application (no window), but
		// on MacOS, to get a proper log console, a full application must be built.
		bIsBuildingConsoleApplication = Target.Platform != UnrealTargetPlatform.Mac;
	}
}
