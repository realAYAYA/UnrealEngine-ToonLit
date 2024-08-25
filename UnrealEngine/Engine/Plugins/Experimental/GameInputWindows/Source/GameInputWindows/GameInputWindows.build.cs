// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool.Rules
{
	public class GameInputWindows : ModuleRules
	{
		public GameInputWindows(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
                    "ApplicationCore",
                    "Engine",
                    "InputCore",
                    "InputDevice",				
                    "CoreUObject",
                    "DeveloperSettings",
					"GameInputBase",
				}
			);
		}
	}
}