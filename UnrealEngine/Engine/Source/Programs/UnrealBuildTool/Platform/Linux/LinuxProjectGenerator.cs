// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class LinuxProjectGenerator : PlatformProjectGenerator
	{
		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Arguments">Command line arguments passed to the project generator</param>
		/// <param name="Logger">Logger for output</param>
		public LinuxProjectGenerator(CommandLineArguments Arguments, ILogger Logger)
			: base(Arguments, Logger)
		{
		}

		/// <summary>
		/// Enumerate all the platforms that this generator supports
		/// </summary>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.Linux;
			yield return UnrealTargetPlatform.LinuxArm64;
		}

		/// <inheritdoc/>
		public override bool HasVisualStudioSupport(VSSettings InVSSettings)
		{
			return false;
		}

		/// <inheritdoc/>
		public override IList<string> GetSystemIncludePaths(UEBuildTarget InTarget)
		{
			List<string> Result = new List<string>();
			string EngineDirectory = Unreal.EngineDirectory.ToString();
			string? UseLibcxxEnvVarOverride = Environment.GetEnvironmentVariable("UE_LINUX_USE_LIBCXX");
			UnrealArch TargetArchitecture = InTarget.Architectures.SingleArchitecture;
			if (String.IsNullOrEmpty(UseLibcxxEnvVarOverride) || UseLibcxxEnvVarOverride == "1")
			{
				if (TargetArchitecture == UnrealArch.X64 || TargetArchitecture == UnrealArch.Arm64)
				{
					// libc++ include directories
					Result.Add(Path.Combine(EngineDirectory, "Source/ThirdParty/Unix/LibCxx/include/"));
					Result.Add(Path.Combine(EngineDirectory, "Source/ThirdParty/Unix/LibCxx/include/c++/v1"));
				}
			}

			UEBuildPlatformSDK BuildPlatformSdk = UEBuildPlatform.GetSDK(InTarget.Platform)!;
			string? InternalSdkPath = BuildPlatformSdk.GetInternalSDKPath();
			if (InternalSdkPath != null)
			{
				string PlatformSdkVersionString = BuildPlatformSdk.GetInstalledVersion()!;
				string Version = GetLinuxToolchainVersionFromFullString(PlatformSdkVersionString);
				string ClangIncludeDirectory = Path.Combine(InternalSdkPath, "lib/clang/" + Version + "/include/");

				Result.Add(Path.Combine(InternalSdkPath, "include"));
				Result.Add(Path.Combine(InternalSdkPath, "usr/include"));
				Result.Add(ClangIncludeDirectory);
				if (!Directory.Exists(ClangIncludeDirectory))
				{
					Logger.LogWarning("Clang include directory doesn't exist on disk. VersionString={VersionString}, ClangDir={ClangDir}",
						PlatformSdkVersionString, ClangIncludeDirectory);
				}
			}

			return Result;
		}

		/// <summary>
		/// Get clang toolchain version from full version string
		/// v17_clang-10.0.1-centos7 -> 10.0.1
		/// v17_clang-16.0.1-centos7 -> 16
		/// </summary>
		/// <param name="FullVersion">Full clang toolchain version string. Example: "v17_clang-10.0.1-centos7"</param>
		/// <returns>Clang toolchain version. Example: 10.0.1 or 16</returns>
		/// <remarks>Starting with clang 16.x the directory naming changed to include major version only</remarks>
		private static string GetLinuxToolchainVersionFromFullString(string FullVersion)
		{
			string FullVersionPattern = @"^v[0-9]+_.*-(([0-9]+)\.[0-9]+\.[0-9]+)-.*$";
			Regex Regex = new Regex(FullVersionPattern);
			Match Match = Regex.Match(FullVersion);
			if (!Match.Success)
			{
				throw new ArgumentException("Wrong full version string", FullVersion);
			}

			Group MajorVersionGroup = Match.Groups[2];
			CaptureCollection MajorVersionCaptures = MajorVersionGroup.Captures;
			if (MajorVersionCaptures.Count != 1)
			{
				throw new ArgumentException("Multiple regex captures in major version string", FullVersion);
			}

			if (Int32.TryParse(MajorVersionCaptures[0].Value, out int MajorVersion))
			{
				if (MajorVersion >= 16)
				{
					return MajorVersionCaptures[0].Value;
				}
			}

			Group FullNumberVersionGroup = Match.Groups[1];
			CaptureCollection FullNumberVersionCaptures = FullNumberVersionGroup.Captures;
			return FullNumberVersionCaptures[0].Value;
		}
	}
}
