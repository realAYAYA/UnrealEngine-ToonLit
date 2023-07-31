// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkComponents : ModuleRules
	{
		public LiveLinkComponents(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"LiveLinkInterface",
			});

			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CinematicCamera",
			});

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Slate",
					"SlateCore",
					"EditorFramework",
					"UnrealEd",
				});
			}
		}
	}
}
