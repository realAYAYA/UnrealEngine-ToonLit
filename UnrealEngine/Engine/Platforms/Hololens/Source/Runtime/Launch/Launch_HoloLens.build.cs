// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Launch_HoloLens : Launch
	{
		public Launch_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			DynamicallyLoadedModuleNames.Add("D3D11RHI");
			DynamicallyLoadedModuleNames.Add("AudioMixerXAudio2");
		}
	}
}
