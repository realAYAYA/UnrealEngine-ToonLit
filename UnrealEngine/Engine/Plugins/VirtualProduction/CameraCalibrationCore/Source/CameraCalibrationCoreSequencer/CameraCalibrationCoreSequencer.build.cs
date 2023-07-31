// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CameraCalibrationCoreSequencer : ModuleRules
{
	public CameraCalibrationCoreSequencer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicIncludePaths.AddRange(
			new string[] {
			}
			);
				
		
		PrivateIncludePaths.AddRange(
			new string[] {
			}
		);
			
		
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
			}
		);
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
 				"CameraCalibrationCore",
 				"CameraCalibrationCoreMovieScene",
 				"Core",
 				"CoreUObject",
 				"LevelSequence",
 				"MovieScene",
 				"MovieSceneTools",
 				"Sequencer",
 				"Slate",
 				"SlateCore",
 				"UnrealEd",
 				"TakeTrackRecorders",
			}
		);
	}
}
