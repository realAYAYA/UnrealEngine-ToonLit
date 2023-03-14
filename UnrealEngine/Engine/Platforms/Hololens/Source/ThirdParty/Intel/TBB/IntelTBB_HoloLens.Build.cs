// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class IntelTBB_HoloLens : IntelTBB
	{
		protected override bool bUseWinTBB { get => true; }
		protected override bool bIncludeDLLRuntimeDependencies { get => false; }

		public IntelTBB_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
		}
	}
}
