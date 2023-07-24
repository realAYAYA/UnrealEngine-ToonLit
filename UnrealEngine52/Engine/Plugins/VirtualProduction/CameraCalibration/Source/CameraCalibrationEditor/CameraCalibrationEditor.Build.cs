// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CameraCalibrationEditor : ModuleRules
	{
		public CameraCalibrationEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"AppFramework",
					"AssetRegistry",
					"AssetTools",
					"CameraCalibrationCore",
					"CinematicCamera",
					"Composure",
					"ComposureLayersEditor",
					"Core",
					"CoreUObject",
	                "CurveEditor",
					"DesktopPlatform",
					"EditorStyle",
					"EditorWidgets",
					"Engine",
					"ImageCore",
					"InputCore",
					"Json",
					"JsonUtilities",
					"MediaAssets",
					"MediaFrameworkUtilities",
					"OpenCV",
					"OpenCVHelper",
					"PlacementMode",
					"PropertyEditor",
					"Settings",
					"Slate",
					"SlateCore",
					"ToolMenus",
					"ToolWidgets",
					"UnrealEd",
					"WorkspaceMenuStructure",
				});
		}
	}
}
