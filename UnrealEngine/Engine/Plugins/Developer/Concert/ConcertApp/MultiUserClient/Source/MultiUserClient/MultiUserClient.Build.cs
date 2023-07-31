// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MultiUserClient : ModuleRules
	{
		public MultiUserClient(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"Concert",
					"ConcertClientSharedSlate",
					"ConcertSharedSlate",
					"ConcertSyncClient",
					"ConcertSyncCore",
					"ConcertTransport",
					"DesktopPlatform",
					"EditorStyle",
					"InputCore",
					"Projects",
					"MessageLog",
					"Slate",
					"SlateCore",
					"SourceControl",
					"ToolMenus",
					"ToolWidgets"
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"EditorFramework",
						"UnrealEd",
						"Sequencer",
						"ToolMenus",
						"WorkspaceMenuStructure",
					}
				);
			}
		}
	}
}
