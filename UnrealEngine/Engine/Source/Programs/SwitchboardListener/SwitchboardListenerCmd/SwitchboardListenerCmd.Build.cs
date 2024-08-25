// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SwitchboardListenerCmd : ModuleRules
{
	public SwitchboardListenerCmd(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"SblCore",
			}
		);

		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"Launch",
			}
		);
	}
}
