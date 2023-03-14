// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class HLMedia_HoloLens : HLMedia
	{
		public HLMedia_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			var EngineDir = Path.GetFullPath(Target.RelativeEnginePath);
			PrivateIncludePaths.Add(Path.Combine(EngineDir, "Platforms/HoloLens/Source/Runtime/Windows/D3D11RHI/Private"));
		}
	}
}
