// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CameraCalibrationCoreEditor : ModuleRules
	{
		public CameraCalibrationCoreEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CameraCalibrationCore",
					"Core",
					"CoreUObject",
					"Engine",
					"PlacementMode",
					"PropertyEditor",
					"Slate",
					"SlateCore",
					"UnrealEd",
				});
		}
	}
}
