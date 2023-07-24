// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProxyTableEditor : ModuleRules
	{
		public ProxyTableEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AssetTools",
					"Chooser",
					"ChooserEditor",
					"ProxyTable",
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