// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RigLogicEditor : ModuleRules
	{
		public RigLogicEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"ControlRig",
					"UnrealEd",
					"EditorFramework",
					"MainFrame",
					"RigLogicModule",
					"RigLogicLib",
					"PropertyEditor",
					"SlateCore",
					"ApplicationCore",
					"Slate",
					"InputCore",
					"EditorWidgets",
					"DesktopPlatform"
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"Settings"
				}
			);
		}
	}
}
