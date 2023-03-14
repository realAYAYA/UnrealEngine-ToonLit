// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InputDebugging : ModuleRules
	{
		public InputDebugging(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Engine",
					"SlateCore",
					"Slate",
					"InputCore",
				}
			);
		}
	}
}