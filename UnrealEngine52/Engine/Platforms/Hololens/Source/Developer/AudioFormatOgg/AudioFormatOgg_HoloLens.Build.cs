// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioFormatOgg_HoloLens : AudioFormatOgg
	{
		protected override bool bWithOggVorbis { get => true; }

		public AudioFormatOgg_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
		}
	}
}
