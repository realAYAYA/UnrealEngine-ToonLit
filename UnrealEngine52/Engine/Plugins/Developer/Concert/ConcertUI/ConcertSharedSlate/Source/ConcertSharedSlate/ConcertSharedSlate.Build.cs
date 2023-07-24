// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertSharedSlate : ModuleRules
	{
		public ConcertSharedSlate(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Concert",
					"Core",
					"CoreUObject",
					"Slate",
					"UndoHistory",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Concert",
					"ConcertTransport",
					"ConcertSyncCore",
					"InputCore",
					"Json",
					"Projects",
					"SlateCore",
					"UndoHistory",
					"ToolWidgets"
				}
			);

		}
	}
}
