// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class FreeType2_HoloLens : FreeType2
	{
		public FreeType2_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			string PlatformSubpath = "Win64";
			string LibPath;

			LibPath = Path.Combine(FreeType2LibPath, PlatformSubpath,
					"VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName());
			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64)
			{
				LibPath = Path.Combine(LibPath, Target.WindowsPlatform.GetArchitectureSubpath());
			}

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "freetype26MT.lib"));

		}
	}
}
