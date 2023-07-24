// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class AddContentDialog : ModuleRules
{
	public AddContentDialog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(
			new string[] {
				"AssetTools",
				"ContentBrowser"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
				"DirectoryWatcher",
				"EditorFramework",
				"Engine",
				"ImageWrapper",
				"InputCore",
				"Json",
				"PakFile",
				"Slate",
				"SlateCore",
				"ToolWidgets",
				"UnrealEd",
				"WidgetCarousel"
			}
		);
	}
}
