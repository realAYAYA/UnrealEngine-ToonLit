// Copyright Epic Games, Inc. All Rights Reserved.


// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class HordeTest : ModuleRules
	{
		public HordeTest(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"Core",
				"Horde",
				"DesktopPlatform",
				"HTTP"
				}
			);
		}
	}
}