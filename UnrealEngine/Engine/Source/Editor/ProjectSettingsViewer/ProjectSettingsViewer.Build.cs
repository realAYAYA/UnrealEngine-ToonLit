// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProjectSettingsViewer : ModuleRules
	{
		public ProjectSettingsViewer(ReadOnlyTargetRules Target) : base(Target)
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
					"CoreUObject",
					"Engine",
					"EngineSettings",
					"SettingsEditor",
					"Slate",
					"SlateCore",
					"EditorFramework",
					"UnrealEd",
					"MoviePlayer",
					"NavigationSystem",
					"AIModule",
					"DeveloperToolSettings",
				}
			);

			if (Target.bBuildTargetDeveloperTools)
			{
				PrivateDependencyModuleNames.Add("ProjectTargetPlatformEditor");
			}
		}
	}
}
