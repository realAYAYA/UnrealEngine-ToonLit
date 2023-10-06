// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UndoHistoryEditor : ModuleRules
	{
		public UndoHistoryEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"CoreUObject",
					"InputCore",
					"Slate",
					"SlateCore",
					"ToolWidgets",
					"UndoHistory",
					"UnrealEd",
				}
			);
		}
	}
}
