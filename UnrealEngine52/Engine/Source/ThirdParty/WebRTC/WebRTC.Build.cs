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
			(Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) && Target.Architecture == UnrealArch.X64);
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
			string WebRtcSdkPath = Target.UEThirdPartySourceDirectory + Path.Join("WebRTC", "4664");
			string VS2013Friendly_WebRtcSdkPath = Target.UEThirdPartySourceDirectory;

			string PlatformSubdir = Target.Platform.ToString();

			string IncludePath = Path.Combine(WebRtcSdkPath, "Include");
			PublicSystemIncludePaths.Add(IncludePath);

			string AbslthirdPartyIncludePath = Path.Combine(IncludePath, "third_party", "abseil-cpp");
			PublicSystemIncludePaths.Add(AbslthirdPartyIncludePath);

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
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "libOpus");

			}
			else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
			{
				PublicDefinitions.Add("WEBRTC_LINUX=1");
				PublicDefinitions.Add("WEBRTC_POSIX=1");

				// This is slightly different than the other platforms
				string LibraryPath = Path.Combine(WebRtcSdkPath, "Lib", PlatformSubdir, Target.Architecture.LinuxName, ConfigPath);
				PublicAdditionalLibraries.Add(Path.Combine(LibraryPath, "libwebrtc.a"));
				
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "libOpus");
			}
		}

		PublicDefinitions.Add("ABSL_ALLOCATOR_NOTHROW=1");
	}
}
