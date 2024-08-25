// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkHubEditor : ModuleRules
	{
		public LiveLinkHubEditor(ReadOnlyTargetRules Target) : base(Target)
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
				"Engine",
				"LiveLink",
				"LiveLinkEditor",
				"LiveLinkHub",
				"LiveLinkHubMessaging",
				"LiveLinkInterface",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd"
			});
		}
	}
}
