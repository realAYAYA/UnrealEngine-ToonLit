// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ConcertClientSharedSlate : ModuleRules
	{
		public ConcertClientSharedSlate(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Slate",
					"SlateCore",
					"EditorStyle",
					"ConcertSharedSlate",
					"ConcertSyncClient",
				}
			);
		}
	}
}
