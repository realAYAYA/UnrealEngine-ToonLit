// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InterchangeEditorPipelines : ModuleRules
	{
		public InterchangeEditorPipelines(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InterchangeCore",
					"InterchangeEngine"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AssetTools",
					"InputCore",
					"InterchangePipelines",
					"MainFrame",
					"Projects",
					"PropertyEditor",
					"Slate",
					"SlateCore",
					"ToolWidgets",
					"UnrealEd"
				}
			);
		}
	}
}
