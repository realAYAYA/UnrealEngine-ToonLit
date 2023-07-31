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
					"Engine",
					"EditorStyle",
					"InputCore",
					"InterchangeTests",
					"LevelEditor",
					"PropertyEditor",
					"UnrealEd"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"WorkspaceMenuStructure",
					"SlateCore",
					"Slate"
				}
			);
		}
    }
}
