// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ChooserEditor : ModuleRules
	{
		public ChooserEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"AssetDefinition",
					"Chooser",
					"UnrealEd",
					"EditorWidgets",
					"ToolWidgets",
					"SlateCore",
					"Slate",
					"PropertyEditor",
					"InputCore",
					"EditorStyle",
					"PropertyEditor",
					"BlueprintGraph",
					"GraphEditor",
					"GameplayTags",
					"GameplayTagsEditor",
					"StructUtils"
					// ... add private dependencies that you statically link with here ...
				}
			);
		}
	}
}