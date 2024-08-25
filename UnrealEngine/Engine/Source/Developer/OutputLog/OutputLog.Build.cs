// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OutputLog : ModuleRules
{
	public OutputLog(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject", // @todo Mac: for some reason it's needed to link in debug on Mac
				"EngineSettings",
				"InputCore",
				"Slate",
				"SlateCore",
				"TargetPlatform",
				"DesktopPlatform",
				"ToolWidgets",
				"ToolMenus",
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"EditorFramework",
					"StatusBar",
					"UnrealEd",
				}
			);
		}

		if (Target.bBuildEditor || Target.bBuildDeveloperTools)
		{
			PrivateIncludePathModuleNames.AddRange(
				new string[] {
					"WorkspaceMenuStructure",
				}
			);
		}

		if (Target.bCompileAgainstEngine)
		{
			// Required for output log drawer in editor / engine builds. 
			PrivateDependencyModuleNames.Add("Engine");
		}

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
