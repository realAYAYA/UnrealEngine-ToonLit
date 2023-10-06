// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InputDevice : ModuleRules
{
    public InputDevice(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange( new string[] { "Core", "Engine" } );
	}
}
