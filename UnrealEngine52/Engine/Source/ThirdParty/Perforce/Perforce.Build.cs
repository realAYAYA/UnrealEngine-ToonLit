// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Perforce : ModuleRules
{
	public Perforce(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			// SDK downloaded at http://ftp.perforce.com/perforce/r21.2/bin.ntx64/p4api_vs2015_dyn_openssl1.1.1.zip
			string Windows_P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2021.2/";

			string PlatformSubdir = Target.Platform.ToString();
			string VisualStudioVersionFolder = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			string IncludeFolder = Path.Combine(Windows_P4APIPath, "Include", PlatformSubdir, VisualStudioVersionFolder);
			PublicSystemIncludePaths.Add(IncludeFolder);

			string ConfigPath = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";
			string LibFolder = Path.Combine(Windows_P4APIPath, "Lib", PlatformSubdir, VisualStudioVersionFolder, ConfigPath);
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "libclient.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "librpc.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "libsupp.lib"));
			
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "libp4api.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "libp4script.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "libp4script_c.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "libp4script_curl.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibFolder, "libp4script_sqlite.lib"));
		}
		else
		{
			string LibFolder = "Lib/";
			string IncludeName = "Include";
			string IncludeSuffix = "";
			string LibPrefix = "";
			string LibPostfixAndExt = ".";
			string P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2015.2/";

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				// the Mac libs are universal libs built from source
				P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2022.1/";
				LibFolder += "Mac";
				IncludeSuffix += "/Mac";
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				P4APIPath = Target.UEThirdPartySourceDirectory + "Perforce/p4api-2018.1/";
				LibFolder += "Linux";
				IncludeSuffix += "/Linux";
			}

			LibPrefix = P4APIPath + LibFolder + "/";
			LibPostfixAndExt = ".a";

			PublicSystemIncludePaths.Add(P4APIPath + IncludeName + IncludeSuffix);
			PublicAdditionalLibraries.Add(LibPrefix + "libclient" + LibPostfixAndExt);

			if (Target.Platform != UnrealTargetPlatform.Win64 && Target.Platform != UnrealTargetPlatform.Mac)
			{
				PrivateDependencyModuleNames.Add("SSL");
			}

			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				PublicAdditionalLibraries.Add(LibPrefix + "libp4api" + LibPostfixAndExt);
				PublicAdditionalLibraries.Add(LibPrefix + "libp4script" + LibPostfixAndExt);
				PublicAdditionalLibraries.Add(LibPrefix + "libp4script_c" + LibPostfixAndExt);
				PublicAdditionalLibraries.Add(LibPrefix + "libp4script_curl" + LibPostfixAndExt);
				PublicAdditionalLibraries.Add(LibPrefix + "libp4script_sqlite" + LibPostfixAndExt);
			}

			PublicAdditionalLibraries.Add(LibPrefix + "librpc" + LibPostfixAndExt);
			PublicAdditionalLibraries.Add(LibPrefix + "libsupp" + LibPostfixAndExt);
		}
	}
}
