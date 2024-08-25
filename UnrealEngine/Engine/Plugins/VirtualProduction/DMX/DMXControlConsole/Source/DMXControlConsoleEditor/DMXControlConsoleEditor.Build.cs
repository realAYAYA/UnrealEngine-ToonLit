// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DMXControlConsoleEditor : ModuleRules
{
	public DMXControlConsoleEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		ShortName = "DMXCtrlConsoleEditor";

		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{              
				"AssetDefinition",
				"AssetRegistry",
				"ContentBrowser",
				"CoreUObject",
				"DMXControlConsole",
				"DMXEditor",
				"DMXProtocol",
				"DMXProtocolEditor",
				"DMXRuntime",
				"Engine",
				"InputCore",
				"LevelEditor",
				"Projects",
				"PropertyEditor",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
			}
		);
	}
}
