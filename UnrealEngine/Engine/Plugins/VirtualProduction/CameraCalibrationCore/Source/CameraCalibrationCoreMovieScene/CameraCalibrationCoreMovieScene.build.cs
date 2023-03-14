// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CameraCalibrationCoreMovieScene : ModuleRules
{
	public CameraCalibrationCoreMovieScene(ReadOnlyTargetRules Target) : base(Target)
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
 				"Core",
 				"CoreUObject",
 				"Engine",
 				"MovieScene",
			}
		);
	}
}
