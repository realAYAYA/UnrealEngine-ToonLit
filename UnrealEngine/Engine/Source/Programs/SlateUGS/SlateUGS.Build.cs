// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SlateUGS : ModuleRules
{
	public SlateUGS(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicIncludePathModuleNames.Add("Launch");

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"UGSCore",
				"AppFramework",
				"Core",
				"ApplicationCore",
				"Projects",
				"Slate",
				"SlateCore",
				"StandaloneRenderer",
				"ToolWidgets",
				"PakFile",
				"DesktopPlatform", // Open file dialog
			}
		);

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"UnixCommonStartup"
				}
			);
		}
	}
}
