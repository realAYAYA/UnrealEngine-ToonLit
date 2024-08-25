// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkHub : ModuleRules
	{
		public LiveLinkHub(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
			});

			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore",
				"AssetRegistry",
				"AssetTools",
				"ContentBrowser",
				"ContentBrowserAssetDataSource",
				"ContentBrowserData",
				"EditorStyle",
				"Engine",
				"InputCore",
				"Json",
				"JsonUtilities",
				"LiveLink",
				"LiveLinkEditor",
				"LiveLinkHubMessaging",
				"LiveLinkInterface",
				"LiveLinkMessageBusFramework",
				"Slate",
				"SlateCore",
				"StandaloneRenderer",
				"StructUtils",
				"TimeManagement",
				"ToolMenus",
				"ToolWidgets",
				"UnrealEd",
				"OutputLog",
			});
		}
	}
}
