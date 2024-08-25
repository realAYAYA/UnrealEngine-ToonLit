// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LensComponentEditor : ModuleRules
	{
		public LensComponentEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CameraCalibrationCore",
					"CameraCalibrationCoreEditor",
					"Core",
					"CoreUObject",
					"Engine",
					"LensComponent",
					"LevelSequence",
					"MovieScene",
					"MovieSceneTools",
					"Sequencer",
					"Slate",
					"SlateCore",
					"TakeTrackRecorders",
					"UnrealEd",
				}
			);
		}
	}
}
