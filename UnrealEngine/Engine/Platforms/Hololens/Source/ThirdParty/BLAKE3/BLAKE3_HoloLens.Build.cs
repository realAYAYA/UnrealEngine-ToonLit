// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class BLAKE3_HoloLens : BLAKE3
	{
		public BLAKE3_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "HoloLens", "arm64", "Release", "BLAKE3.lib"));
			}
			else if (Target.WindowsPlatform.Architecture == WindowsArchitecture.x64)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "HoloLens", "x64", "Release", "BLAKE3.lib"));
			}

		}
	}
}
