// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class zlib_HoloLens : zlib
	{
		public zlib_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.x64)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", "Release", "zlibstatic.lib"));
			}
			else
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "Win64", Target.WindowsPlatform.GetArchitectureSubpath(), "Release", "zlibstatic.lib"));
			}
		}
	}
}
