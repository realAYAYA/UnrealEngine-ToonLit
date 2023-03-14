// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class InputDevice : ModuleRules
{
    public InputDevice(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePaths.AddRange(
			new string[] {
				"Runtime/InputDevice/Public"
			}
			);

		PrivateDependencyModuleNames.AddRange( new string[] { "Core", "CoreUObject", "Engine" } );
	}
}
