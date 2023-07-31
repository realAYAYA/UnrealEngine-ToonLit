// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MicrosoftSpatialSound_HoloLens : MicrosoftSpatialSound
	{
		protected override bool bSupportedPlatform { get => true; }
		protected override string Platform { get => "HoloLens"; }
		public MicrosoftSpatialSound_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
		}
	}
}
