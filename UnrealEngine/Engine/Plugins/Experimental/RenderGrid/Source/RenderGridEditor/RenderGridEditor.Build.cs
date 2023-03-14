// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RenderGridEditor : ModuleRules
{
	public RenderGridEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"RemoteControl",
			}
		);


		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AppFramework",
				"AssetTools",
				"DesktopPlatform",
				"EditorStyle",
				"GraphEditor",
				"InputCore",
				"Kismet",
				"LevelSequence",
				"MovieRenderPipelineCore",
				"MovieScene",
				"Projects",
				"PropertyEditor",
				"RenderGrid",
				"RenderGridDeveloper",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
			}
		);
	}
}