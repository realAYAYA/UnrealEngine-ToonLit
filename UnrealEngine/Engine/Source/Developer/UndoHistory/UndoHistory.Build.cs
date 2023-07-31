// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class UndoHistory : ModuleRules
	{
		public UndoHistory(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"ApplicationCore",
					"CoreUObject",
					"InputCore",
					"Slate",
					"SlateCore",
				}
			);
		}
	}
}
