// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DatasmithContentEditor : ModuleRules
	{
		public DatasmithContentEditor(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DatasmithContent",
					"DesktopPlatform",
					"EditorFramework",
					"Engine",
					"Projects",
					"UnrealEd",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"InputCore",
					"PropertyEditor",
					"SlateCore",
					"Slate",
					"ToolMenus",
					"DetailCustomizations",
                }
			);
		}
	}
}