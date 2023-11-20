// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libcurl : ModuleRules
{
	public libcurl(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_LIBCURL=1");
		PublicDefinitions.Add("CURL_STATICLIB=1");

		string LibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/8.4.0/";
		PublicSystemIncludePaths.Add(Path.Combine(LibCurlPath, "include"));

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibCurlPath, "lib", "Unix", Target.Architecture.LinuxName, "Release", "libcurl.a"));
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			string[] Architectures = new string[] {
				"ARM64",
				"x64",
			};
 
			foreach(var Architecture in Architectures)
			{
				PublicAdditionalLibraries.Add(Path.Combine(LibCurlPath, "lib", "Android", Architecture, "Release", "libcurl.a"));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibCurlPath, "lib", "Mac", "Release", "libcurl.a"));
			PublicFrameworks.Add("SystemConfiguration");
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64 && !Target.WindowsPlatform.bUseXCurl)
		{
			PublicAdditionalLibraries.Add(Path.Combine(LibCurlPath, "lib", Target.Platform.ToString(), "Release", "libcurl.lib"));
		}

		// Our build requires nghttp2, OpenSSL and zlib, so ensure they're linked in
		AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
		{
			"nghttp2",
			"OpenSSL",
			"zlib"
		});
	}
}
