// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class libcurl : ModuleRules
{
	public libcurl(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicDefinitions.Add("WITH_LIBCURL=1");

		string LinuxLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/7.83.1/";
		string MacLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/7.83.1/";
		string WinLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/7.83.1/";
		string AndroidLibCurlPath = Target.UEThirdPartySourceDirectory + "libcurl/7_75_0/";

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			PublicSystemIncludePaths.Add(Path.Combine(LinuxLibCurlPath, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(LinuxLibCurlPath, "lib", "Unix", Target.Architecture.LinuxName, "Release", "libcurl.a"));
			PublicDefinitions.Add("CURL_STATICLIB=1");

			// Our build requires nghttp2, OpenSSL and zlib, so ensure they're linked in
			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"nghttp2",
				"OpenSSL",
				"zlib"
			});
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Android))
		{
			string[] Architectures = new string[] {
				"ARM64",
				"x64",
			};
 
			PublicSystemIncludePaths.Add(AndroidLibCurlPath + "include/Android/");
			foreach(var Architecture in Architectures)
			{
				PublicAdditionalLibraries.Add(AndroidLibCurlPath + "lib/Android/" + Architecture + "/libcurl.a");
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			PublicSystemIncludePaths.Add(Path.Combine(MacLibCurlPath, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(MacLibCurlPath, "lib", "Mac", "Release", "libcurl.a"));
			PublicDefinitions.Add("CURL_STATICLIB=1");
			PublicFrameworks.Add("SystemConfiguration");

			// Our build requires nghttp2, OpenSSL and zlib, so ensure they're linked in
			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"nghttp2",
				"OpenSSL",
				"zlib"
			});
		}
		else if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicSystemIncludePaths.Add(Path.Combine(WinLibCurlPath, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(WinLibCurlPath, "lib", Target.Platform.ToString(), "Release", "libcurl.lib"));
			PublicDefinitions.Add("CURL_STATICLIB=1");

			// Our build requires nghttp2, OpenSSL and zlib, so ensure they're linked in
			AddEngineThirdPartyPrivateStaticDependencies(Target, new string[]
			{
				"nghttp2",
				"OpenSSL",
				"zlib"
			});
		}
	}
}
