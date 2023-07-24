// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MfMedia_HoloLens : MfMedia
	{
		public MfMedia_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDefinitions.Add("MFMEDIA_SUPPORTED_PLATFORM=1");
			PrivateDefinitions.Add("MFMEDIA_NEED_PLATFORM_PRIVATE=0");
		}
	}
}
