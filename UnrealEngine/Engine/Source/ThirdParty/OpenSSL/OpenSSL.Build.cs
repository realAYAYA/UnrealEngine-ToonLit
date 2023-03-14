// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class OpenSSL : ModuleRules
{
	public OpenSSL(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		string OpenSSL111cPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.1.1c");
		string OpenSSL111kPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.1.1k");
		string OpenSSL111nPath = Path.Combine(Target.UEThirdPartySourceDirectory, "OpenSSL", "1.1.1n");

		string PlatformSubdir = Target.Platform.ToString();
		string ConfigFolder = (Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT) ? "Debug" : "Release";

		if (Target.Platform == UnrealTargetPlatform.Mac || Target.Platform == UnrealTargetPlatform.IOS)
		{
			PublicIncludePaths.Add(Path.Combine(OpenSSL111nPath, "include", PlatformSubdir));

			string LibPath = Path.Combine(OpenSSL111nPath, "lib", PlatformSubdir);

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.a"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.a"));
		}
		else if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			PlatformSubdir = "Win64";
			string VSVersion = "VS" + Target.WindowsPlatform.GetVisualStudioCompilerVersionName();

			// Add includes
			PublicIncludePaths.Add(Path.Combine(OpenSSL111nPath, "include", PlatformSubdir, VSVersion));

			// Add Libs
			string LibPath = Path.Combine(OpenSSL111nPath, "lib", PlatformSubdir, VSVersion, ConfigFolder);

			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libssl.lib"));
			PublicAdditionalLibraries.Add(Path.Combine(LibPath, "libcrypto.lib"));
			PublicSystemLibraries.Add("crypt32.lib");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Unix))
		{
			string platform = "/Unix/" + Target.Architecture;
			string IncludePath = OpenSSL111nPath + "/include" + platform;
			string LibraryPath = OpenSSL111nPath + "/lib" + platform;

			PublicIncludePaths.Add(IncludePath);
			PublicAdditionalLibraries.Add(LibraryPath + "/libssl.a");
			PublicAdditionalLibraries.Add(LibraryPath + "/libcrypto.a");
		}
		else if (Target.Platform == UnrealTargetPlatform.Android)
		{
			string[] Architectures = new string[] {
				"ARM64",
				"x86",
				"x64",
			};

			PublicIncludePaths.Add(OpenSSL111nPath + "/include/Android/");

			foreach(var Architecture in Architectures)
			{
				PublicAdditionalLibraries.Add(OpenSSL111nPath + "/lib/Android/" + Architecture + "/libcrypto.a");
				PublicAdditionalLibraries.Add(OpenSSL111nPath + "/lib/Android/" + Architecture + "/libssl.a");
			}
		}
	}
}
