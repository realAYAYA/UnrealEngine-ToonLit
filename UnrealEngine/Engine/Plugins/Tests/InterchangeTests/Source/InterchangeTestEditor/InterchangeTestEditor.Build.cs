// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class InterchangeTestEditor : ModuleRules
	{
		public InterchangeTestEditor(ReadOnlyTargetRules Target) : base(Target)
		{
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
                    "Core",
					"CoreUObject",
					"ContentBrowser",
					"Engine",
					"EditorStyle",
					"InputCore",
					"InterchangeTests",
					"LevelEditor",
					"PropertyEditor",
					"ToolMenus",
					"UnrealEd"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetDefinition",
					"AssetTools",
					"WorkspaceMenuStructure",
					"SlateCore",
					"Slate"
				}
			);
		}
    }
}
