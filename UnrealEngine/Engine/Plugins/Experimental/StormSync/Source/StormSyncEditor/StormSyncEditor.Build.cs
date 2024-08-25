// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class StormSyncEditor : ModuleRules
{
	public StormSyncEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AppFramework",
				"AssetTools",
				"CoreUObject",
				"ContentBrowserData",
				"ContentBrowserAssetDataSource",
				"DesktopPlatform",
				"EditorFramework",
				"EditorSubsystem",
				"Engine",
				"InputCore",
				"MessageLog",
				"RenderCore",
				"Slate",
				"SlateCore",
                "Sockets",
				"StormSyncCore",
				"StormSyncTransportCore",
				"StormSyncTransportClient",
				"StormSyncTransportServer",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
			}
		);
	}
}