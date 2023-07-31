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

		if (Target.Type == TargetType.Editor || Target.Type == TargetType.Program)
		{
			PrivateDefinitions.Add("VIRTUALCAMERA_WITH_CONCERT=1");
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Concert",
					"ConcertSyncClient",
					"MultiUserClient",
				}
			);
		}
		else
		{
			PrivateDefinitions.Add("VIRTUALCAMERA_WITH_CONCERT=0");
		}


		if (Target.bBuildEditor == true)
		{
			PublicDependencyModuleNames.Add("LevelSequenceEditor");
			PublicDependencyModuleNames.Add("Sequencer");
			PublicDependencyModuleNames.Add("SlateCore");
			PublicDependencyModuleNames.Add("TakeRecorder");
			PrivateDependencyModuleNames.Add("LevelEditor");
			PrivateDependencyModuleNames.Add("EditorFramework");
			PrivateDependencyModuleNames.Add("UnrealEd");
			PrivateDependencyModuleNames.Add("EditorScriptingUtilities");
			PrivateDependencyModuleNames.Add("VPUtilitiesEditor");
			PrivateDependencyModuleNames.Add("TakesCore");
		}
	}
}
