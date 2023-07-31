// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libOpus : ModuleRules
{
	protected virtual string OpusVersion	  { get { return "opus-1.1"; } }
	protected virtual string IncRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }
	protected virtual string LibRootDirectory { get { return Target.UEThirdPartySourceDirectory; } }

	protected virtual string OpusIncPath { get { return Path.Combine(IncRootDirectory, "libOpus", OpusVersion, "include"); } }
	protected virtual string OpusLibPath { get { return Path.Combine(LibRootDirectory, "libOpus", OpusVersion); } }

	public libOpus(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string LibraryPath = OpusLibPath + "/";
		bool IsWinPlatform = Target.Platform == UnrealTargetPlatform.Win64;
		string OpusLibraryPath = Path.Combine(LibRootDirectory, "libOpus", "opus-1.3.1-12"); 

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			LibraryPath += "Windows/VS2012/x64/Release/";

 			//PublicAdditionalLibraries.Add(LibraryPath + "silk_common.lib");
 			//PublicAdditionalLibraries.Add(LibraryPath + "silk_float.lib");
 			//PublicAdditionalLibraries.Add(LibraryPath + "celt.lib");
			//PublicAdditionalLibraries.Add(LibraryPath + "opus.lib");
			PublicAdditionalLibraries.Add(LibraryPath + "speex_resampler.lib");

			string ConfigPath = "";
			if (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT)
			{
				ConfigPath = "Debug";
			}
			else
			{
				ConfigPath = "Release";
			}

			string OpusBinaryPath = Path.Combine(OpusLibraryPath, "bin", Target.Platform.ToString(), ConfigPath);
			PublicAdditionalLibraries.Add(Path.Combine(OpusBinaryPath, "opus.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(OpusBinaryPath, "opus_sse41.lib"));
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string OpusPath = LibraryPath + "/Mac/libopus.a";
			string SpeexPath = LibraryPath + "/Mac/libspeex_resampler.a";

			PublicAdditionalLibraries.Add(OpusPath);
			PublicAdditionalLibraries.Add(SpeexPath);
		}
        else if (Target.Platform == UnrealTargetPlatform.IOS)
        {
            string OpusPath = LibraryPath + "/IOS/libOpus.a";
            PublicAdditionalLibraries.Add(OpusPath);
        }
	else if (Target.Platform == UnrealTargetPlatform.TVOS)
        {
            string OpusPath = LibraryPath + "/TVOS/libOpus.a";
            PublicAdditionalLibraries.Add(OpusPath);
        }
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
            if (Target.LinkType == TargetLinkType.Monolithic)
            {
                PublicAdditionalLibraries.Add(LibraryPath + "Unix/" + Target.Architecture + "/libopus.a");
            }
            else
            {
                PublicAdditionalLibraries.Add(LibraryPath + "Unix/" + Target.Architecture + "/libopus_fPIC.a");
            }

			if (Target.Architecture.StartsWith("x86_64"))
			{
				if (Target.LinkType == TargetLinkType.Monolithic)
				{
					PublicAdditionalLibraries.Add(LibraryPath + "Unix/" + Target.Architecture + "/libresampler.a");
				}
				else
				{
					PublicAdditionalLibraries.Add(LibraryPath + "Unix/" + Target.Architecture + "/libresampler_fPIC.a");
				}
			}
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			string[] Architectures = new string[] {
				"ARM64",
				"x64"
			};

			foreach(string Architecture in Architectures)
			{
				PublicAdditionalLibraries.Add(LibraryPath + "Android/" + Architecture + "/libopus.a");
				PublicAdditionalLibraries.Add(LibraryPath + "Android/" + Architecture + "/libspeex_resampler.a");
			}
		}

		PublicIncludePaths.Add(IsWinPlatform ? Path.Combine(OpusLibraryPath, "include") : OpusIncPath);
    }
}
