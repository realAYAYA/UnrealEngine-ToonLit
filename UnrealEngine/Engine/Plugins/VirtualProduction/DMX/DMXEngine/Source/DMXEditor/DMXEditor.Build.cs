// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class DMXEditor : ModuleRules
{
	public DMXEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"DMXProtocol",
				"DMXProtocolEditor",
				"DMXRuntime",
				"ToolMenus"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser",
				"CoreUObject",
				"DesktopPlatform",
				"EditorFramework",
				"EditorStyle",
				"EditorWidgets",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"Kismet",
				"KismetWidgets",
				"Projects",
				"MainFrame",
				"MovieScene",
				"PropertyEditor",
				"Sequencer",
				"Slate",
				"SlateCore",
				"TakesCore",
				"TakeRecorder",
				"TakeTrackRecorders",
				"ToolWidgets",
				"UnrealEd",
				"XmlParser",
			}
		);
	}
}
