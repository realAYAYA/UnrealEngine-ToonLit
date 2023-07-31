// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using UnrealBuildTool;

public class libav : ModuleRules
{
	public libav(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.NoPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"Core",
				});


		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "Public"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string IncPath = Path.Combine(ModuleDirectory, "include");
			string LibPath = Path.Combine(ModuleDirectory, "lib", "Linux", Target.Architecture);

			// The files we check for existence to determine whether or not we can enable the
			// use ov libav.
			string [] FilesToProbe = new string [] {
				Path.Combine(IncPath, "libavcodec", "avcodec.h"),
				Path.Combine(IncPath, "libavutil", "avutil.h"),
				Path.Combine(LibPath, "libavcodec.so"),
				Path.Combine(LibPath, "libavutil.so")
			};
			bool bHaveRequiredFiles = true;
			foreach(string Probe in FilesToProbe)
			{
				bool bHaveFile = File.Exists(Probe);
				//Console.WriteLine("checking:" + Probe + " -> " + bHaveFile);
				if (!bHaveFile)
				{
					bHaveRequiredFiles = false;
					break;
				}
			}

			if (bHaveRequiredFiles)
			{
				PrivateIncludePaths.Add(Path.Combine(IncPath));
				PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libavcodec.so"));
				PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libavutil.so"));
				PublicDefinitions.Add("WITH_LIBAV=1");
			}
			else
			{
				PublicDefinitions.Add("WITH_LIBAV=0");
			}
		}
	}
}
