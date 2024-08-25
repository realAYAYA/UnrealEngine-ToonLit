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
					"AssetDefinition",
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
					"DeveloperSettings",
					"EditorStyle",
					"EditorWidgets",
					"Engine",
					"RenderCore",
					"ImageCore",
					"InputCore",
					"Json",
					"JsonUtilities",
					"LensComponent",
					"MediaAssets",
					"MediaFrameworkUtilities",
					"OpenCV",
					"OpenCVHelper",
					"PlacementMode",
					"PropertyEditor",
					"SequencerWidgets",
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
