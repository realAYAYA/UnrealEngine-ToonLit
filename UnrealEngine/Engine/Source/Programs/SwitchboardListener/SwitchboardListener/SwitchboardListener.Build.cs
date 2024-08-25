// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;


public class SwitchboardListener : SwitchboardListenerCmd
{
	public SwitchboardListener(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SblSlate",
			}
		);
	}
}
