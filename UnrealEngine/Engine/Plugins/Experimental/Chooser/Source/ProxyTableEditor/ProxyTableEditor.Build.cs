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
					"AssetDefinition",
					"Chooser",
					"ChooserEditor",
					"ProxyTable",
					"UnrealEd",
					"EditorWidgets",
					"ToolWidgets",
					"ToolMenus",
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
					"StructUtils",
					"StructUtilsEditor",
					"BlendStack"
					// ... add private dependencies that you statically link with here ...
				}
			);
		}
	}
}