// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DisplayClusterConfigurator : ModuleRules
{
	public DisplayClusterConfigurator(ReadOnlyTargetRules ROTargetRules) : base(ROTargetRules)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"DisplayCluster",
				"DisplayClusterConfiguration",
				"DisplayClusterProjection",

				"AdvancedPreviewScene",
				"ApplicationCore",
				"AppFramework",
				"AssetTools",
				"CinematicCamera",
				"Core",
				"CoreUObject",
				"DesktopPlatform",
				"BlueprintGraph",
				"GraphEditor",
				"EditorFramework",
				
				"EditorSubsystem",
				"EditorWidgets",
				"Engine",
				"ImageWrapper",
				"InputCore",
				"Kismet",
				"KismetCompiler",
				"MainFrame",
				"MediaAssets",
				"MessageLog",
				"PinnedCommandList",
				"Projects",
				"PropertyEditor",
				"Serialization",
				"Settings",
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
				"SubobjectEditor",
				"SubobjectDataInterface",
				"ToolWidgets",
				"ProceduralMeshComponent",
			});
	}
}
