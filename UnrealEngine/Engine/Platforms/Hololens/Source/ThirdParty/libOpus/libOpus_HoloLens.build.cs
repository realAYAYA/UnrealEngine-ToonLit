// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class libOpus_HoloLens : libOpus
	{
		public libOpus_HoloLens(ReadOnlyTargetRules Target) : base(Target)
		{
			string LibraryPath = OpusLibPath + "/";

			if (Target.WindowsPlatform.Architecture == WindowsArchitecture.x64)
			{
				LibraryPath += "Windows/VS2012/";
				LibraryPath += "x64/";
			}
			else if (Target.WindowsPlatform.Architecture == WindowsArchitecture.ARM64)
			{
				LibraryPath += "Windows/VS" + (Target.WindowsPlatform.Compiler >= WindowsCompiler.VisualStudio2019 ? "2015" : "2012");
				LibraryPath += "/ARM64/";
			}


			LibraryPath += "Release/";

			PublicAdditionalLibraries.Add(LibraryPath + "silk_common.lib");
			PublicAdditionalLibraries.Add(LibraryPath + "silk_float.lib");
			PublicAdditionalLibraries.Add(LibraryPath + "celt.lib");
			PublicAdditionalLibraries.Add(LibraryPath + "opus.lib");
			PublicAdditionalLibraries.Add(LibraryPath + "speex_resampler.lib");

		}
	}
}
