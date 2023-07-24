// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MediaIOEditor : ModuleRules
	{
		public MediaIOEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"EditorFramework",
					"MediaIOCore",
					"Slate",
					"SlateCore",
					"TimeManagement",
					"UnrealEd",
				});

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Engine",
					"MediaAssets"
				});
		}
	}
}
