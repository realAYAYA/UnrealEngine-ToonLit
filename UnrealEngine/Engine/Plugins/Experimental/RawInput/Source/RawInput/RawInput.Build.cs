// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool.Rules
{
	public class RawInput : ModuleRules
	{
		public RawInput(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"ApplicationCore",
					"InputCore",
					"SlateCore",
					"Slate"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"InputDevice",
					"DeveloperSettings"
				}
			);
		}
	}
}
