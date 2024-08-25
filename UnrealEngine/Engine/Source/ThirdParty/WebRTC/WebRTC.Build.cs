// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;
using System;

public class WebRTC : ModuleRules
{
	protected string ConfigPath {get; private set; }

	protected virtual bool bShouldUseWebRTC
	{
		get =>
			Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
			(Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture == UnrealArch.X64) ||
			Target.Platform == UnrealTargetPlatform.Mac;
	}

	//Config switch to use new WebRTC version 5414 (M109)
	protected virtual bool bShouldUse5414WebRTC
	{
		get =>
			true;
	}

	public WebRTC(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		// WebRTC binaries with debug symbols are huge hence the Release binaries do not have any
		// if you want to have debug symbols with shipping you will need to build with debug instead  
		if (Target.Configuration != UnrealTargetConfiguration.Debug)
		{
			ConfigPath = "Release";
		}
		else
		{
			// The debug webrtc binares are not portable, so we only ship with the release binaries
			// If you wanted, you would need to compile the webrtc binaries in debug and place the Lib and Include folder in the relevant location
			// ConfigPath = "Debug";
			ConfigPath = "Release";
		}

		if (bShouldUseWebRTC)
		{
			string WebRtcSdkPath;
			if (bShouldUse5414WebRTC)
			{
				WebRtcSdkPath = Target.UEThirdPartySourceDirectory + Path.Join("WebRTC", "5414");
				PublicDefinitions.Add("WEBRTC_5414=1");
			}
			else
			{
				WebRtcSdkPath = Target.UEThirdPartySourceDirectory + Path.Join("WebRTC", "4664");
				PublicDefinitions.Add("WEBRTC_5414=0");
			}

			string VS2013Friendly_WebRtcSdkPath = Target.UEThirdPartySourceDirectory;

			string PlatformSubdir = Target.Platform.ToString();

			string IncludePath = Path.Combine(WebRtcSdkPath, "Include");
			PublicSystemIncludePaths.Add(IncludePath);

			// Include our compatiblity headers
			string CompatIncludesPath = Path.Combine(Target.UEThirdPartySourceDirectory, "WebRTC", "CompatInclude");
			PublicSystemIncludePaths.Add(CompatIncludesPath);

			string AbslthirdPartyIncludePath = Path.Combine(IncludePath, "third_party", "abseil-cpp");
			PublicSystemIncludePaths.Add(AbslthirdPartyIncludePath);

			// libyuv is linked inside WebRTC so just use those headers and binaries where avaliable
			string libyuvthirdPartyIncludePath = Path.Combine(IncludePath, "third_party", "libyuv", "include");
			PublicSystemIncludePaths.Add(libyuvthirdPartyIncludePath);

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				PublicDefinitions.Add("WEBRTC_WIN=1");

				PlatformSubdir = "Win64"; // windows-based platform extensions can share this library
				string LibraryPath = Path.Combine(WebRtcSdkPath, "Lib", PlatformSubdir, ConfigPath);
				
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "webrtc.lib"));

				// Additional System library
				PublicSystemLibraries.Add("Secur32.lib");

				// The version of webrtc we depend on, depends on an openssl that depends on zlib
				AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");


			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				PublicDefinitions.Add("WEBRTC_LINUX=1");
				PublicDefinitions.Add("WEBRTC_POSIX=1");

				// This is slightly different than the other platforms
				string LibraryPath = Path.Combine(WebRtcSdkPath, "Lib", PlatformSubdir, Target.Architecture.LinuxName, ConfigPath);
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "libwebrtc.a"));
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac && bShouldUse5414WebRTC)
            {
				// NOTE: We can't use Target.Platform.IsInGroup(UnrealPlatformGroup.Apple) because that includes tvOS and iOS which we don't support
                PublicDefinitions.Add("WEBRTC_MAC=1");
                PublicDefinitions.Add("WEBRTC_POSIX=1");

                string LibraryPath = Path.Combine(WebRtcSdkPath, "Lib", PlatformSubdir, ConfigPath);
                PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "libwebrtc.a"));
            }

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
			AddEngineThirdPartyPrivateStaticDependencies(Target, "libOpus");

			// We remove LibVPX symbols from 5414 and instead depend on the ones provided
			// in engine third party
			if(bShouldUse5414WebRTC)
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "LibVpx");
			}
		}

		PublicDefinitions.Add("ABSL_ALLOCATOR_NOTHROW=1");
	}
}
