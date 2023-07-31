// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AudioFormatOpus_HoloLens : AudioFormatOpus
	{
		protected override bool bWithLibOpus { get => true; }

		public AudioFormatOpus_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
		}
	}
}
