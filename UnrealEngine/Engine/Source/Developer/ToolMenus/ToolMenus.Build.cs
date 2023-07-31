// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ToolMenus : ModuleRules
	{
		public ToolMenus(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"ApplicationCore",
					"Slate",
					"SlateCore",
					"InputCore"
				}
			);
		}
	}
}
