// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StateTreeEditorModule : ModuleRules
	{
		public StateTreeEditorModule(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicIncludePaths.AddRange(
			new string[] {
			}
			);

			PublicDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"AssetTools",
				"EditorFramework",
				"UnrealEd",
				"Slate",
				"SlateCore",
				"EditorStyle",
				"PropertyEditor",
				"StateTreeModule",
				"SourceControl",
				"Projects",
				"BlueprintGraph",
				"PropertyAccessEditor",
				"StructUtils",
				"StructUtilsEngine",
				"StructUtilsEditor",
				"GameplayTags",
			}
			);

			PrivateDependencyModuleNames.AddRange(
			new string[] {
				"AssetDefinition",
				"RenderCore",
				"GraphEditor",
				"KismetWidgets",
				"PropertyPath",
				"ToolMenus",
				"ToolWidgets",
				"ApplicationCore",
				"DeveloperSettings",
				"RewindDebuggerInterface"
			}
			);

			PrivateIncludePathModuleNames.AddRange(new string[] {
				"MessageLog",
			});

			PrivateIncludePaths.AddRange(new string[] {
				System.IO.Path.Combine(GetModuleDirectory("PropertyEditor"), "Private"),
			});
		
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				PublicDefinitions.Add("WITH_STATETREE_DEBUGGER=1");
			}
			else
			{
				PublicDefinitions.Add("WITH_STATETREE_DEBUGGER=0");
			}
		}
	}
}
