// Copyright Epic Games, Inc. All Rights Reserved.
using UnrealBuildTool;

public class SoundSwitch : ModuleRules
{
	public SoundSwitch(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicIncludePaths.Add(Target.UEThirdPartySourceDirectory + "IOS/SoundSwitch/SoundSwitch/SoundSwitch");
		}
	}
}


