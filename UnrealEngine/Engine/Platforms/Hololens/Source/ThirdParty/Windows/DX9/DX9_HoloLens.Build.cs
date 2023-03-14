// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DX9_HoloLens : DX9
	{
		protected override string LibDir { get => null; }

		public DX9_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
		}
	}
}
