// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class HLMediaLibrary_HoloLens : HLMediaLibrary
	{
		protected override string Platform { get => "HoloLens"; }
		protected override bool bSupportedPlatform { get => true; }

		public HLMediaLibrary_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
		}
	}
}
