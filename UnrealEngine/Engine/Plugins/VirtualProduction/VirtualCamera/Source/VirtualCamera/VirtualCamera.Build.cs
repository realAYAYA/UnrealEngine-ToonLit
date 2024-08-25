// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class VirtualCamera : ModuleRules
{
	public VirtualCamera(ReadOnlyTargetRules Target) : base(Target)
	{
		DefaultBuildSettings = BuildSettingsVersion.V2;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AugmentedReality",
				"CinematicCamera",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"LevelSequence",
				"LiveLinkInterface",
				"MovieScene",
				"RemoteSession",
				"TimeManagement",
				"UMG",
				"VCamCore",
				"VPUtilities",
				"AssetRegistry",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"MediaIOCore",
				"Slate",
				"AdvancedWidgets"
			}
		);
		
		if (Target.bBuildEditor == true)
		{
			PublicDependencyModuleNames.AddRange(new string[]
			{
				"LevelSequenceEditor",
				"Sequencer",
				"SlateCore",
				"TakeRecorder"
			});
			
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"ConcertTakeRecorder",
				"EditorFramework",
				"EditorScriptingUtilities",
				"LevelEditor",
				"TakesCore",
				"UnrealEd",
				"VPUtilitiesEditor",
			});
		}
	}
}
