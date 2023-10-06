// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics;
using System.IO;
using System.Runtime.Versioning;
using System.Text;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using Microsoft.Win32;

namespace UnrealBuildBase
{
	static public class ApplePlatformSDK
	{
		public static readonly string? InstalledSDKVersion = GetInstalledSDKVersion();

		public static bool TryConvertVersionToInt(string? StringValue, out UInt64 OutValue)
		{
			OutValue = 0;

			if (StringValue == null)
			{
				return false;
			}

			// 8 bits per component, with high getting extra from high 32
			Match Result = Regex.Match(StringValue, @"^(\d+).(\d+)(.(\d+))?(.(\d+))?(.(\d+))?$");
			if (Result.Success)
			{
				OutValue = UInt64.Parse(Result.Groups[1].Value) << 24 | UInt64.Parse(Result.Groups[2].Value) << 16;
				if (Result.Groups[4].Success)
				{
					OutValue |= UInt64.Parse(Result.Groups[4].Value) << 8;
				}
				if (Result.Groups[6].Success)
				{
					OutValue |= UInt64.Parse(Result.Groups[6].Value) << 0;
				}
				return true;
			}

			return false;
		}

		private static string? GetInstalledSDKVersion()
		{
			// get Xcode version on Mac
			if (OperatingSystem.IsMacOS())
			{
				int ExitCode;
				// xcode-select -p gives the currently selected Xcode location (xcodebuild -version may fail if Xcode.app is broken)
				// Example output: /Applications/Xcode.app/Contents/Developer
				string Output = RunLocalProcessAndReturnStdOut("sh", "-c 'xcode-select -p'", out ExitCode);

				if (ExitCode == 0)
				{
					DirectoryReference DeveloperDir = new DirectoryReference(Output);
					FileReference Plist = FileReference.Combine(DeveloperDir.ParentDirectory!, "Info.plist");
					// Find out the version number in Xcode.app/Contents/Info.plist
					Output = RunLocalProcessAndReturnStdOut("sh",
						  $"-c 'plutil -extract CFBundleShortVersionString raw {Plist}'", out ExitCode);
					if (ExitCode == 0)
					{
						return Output;
					}
				}

				return null;
			}

			if (OperatingSystem.IsWindows())
			{
				// otherwise, get iTunes "Version"
				string? DllPath =
					Registry.GetValue(
						"HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared",
						"iTunesMobileDeviceDLL", null) as string;
				if (string.IsNullOrEmpty(DllPath) || !File.Exists(DllPath))
				{
					DllPath = Registry.GetValue(
						"HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared",
						"MobileDeviceDLL", null) as string;
					if (string.IsNullOrEmpty(DllPath) || !File.Exists(DllPath))
					{
						// iTunes >= 12.7 doesn't have a key specifying the 32-bit DLL but it does have a ASMapiInterfaceDLL key and MobileDevice.dll is in usually in the same directory
						DllPath = Registry.GetValue(
							"HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Apple Inc.\\Apple Mobile Device Support\\Shared",
							"ASMapiInterfaceDLL", null) as string;
						DllPath = String.IsNullOrEmpty(DllPath)
							? null
							: DllPath.Substring(0, DllPath.LastIndexOf('\\') + 1) + "MobileDevice.dll";

						if (string.IsNullOrEmpty(DllPath) || !File.Exists(DllPath))
						{
							DllPath = FindWindowsStoreITunesDLL();
						}
					}
				}

				if (!string.IsNullOrEmpty(DllPath) && File.Exists(DllPath))
				{
					string? DllVersion = FileVersionInfo.GetVersionInfo(DllPath).FileVersion;
					// Only return the DLL version as the SDK version if we can correctly parse it
					if (TryConvertVersionToInt(DllVersion, out _))
					{
						return DllVersion;
					}
				}
			}

			return null;
		}

		[SupportedOSPlatform("windows")]
		private static string? FindWindowsStoreITunesDLL()
		{
			string? InstallPath = null;

			string PackagesKeyName = "Software\\Classes\\Local Settings\\Software\\Microsoft\\Windows\\CurrentVersion\\AppModel\\PackageRepository\\Packages";

			RegistryKey? PackagesKey = Registry.LocalMachine.OpenSubKey(PackagesKeyName);
			if (PackagesKey != null)
			{
				string[] PackageSubKeyNames = PackagesKey.GetSubKeyNames();

				foreach (string PackageSubKeyName in PackageSubKeyNames)
				{
					if (PackageSubKeyName.Contains("AppleInc.iTunes") && (PackageSubKeyName.Contains("_x64") || PackageSubKeyName.Contains("_x86")))
					{
						string FullPackageSubKeyName = PackagesKeyName + "\\" + PackageSubKeyName;

						RegistryKey? iTunesKey = Registry.LocalMachine.OpenSubKey(FullPackageSubKeyName);
						if (iTunesKey != null)
						{
							object? Value = iTunesKey.GetValue("Path");
							if (Value != null)
							{
								InstallPath = (string)Value + "\\AMDS32\\MobileDevice.dll";
							}
							break;
						}
					}
				}
			}

			return InstallPath;
		}
		
		/// <summary>
		/// Runs a command line process, and returns simple StdOut output. This doesn't handle errors or return codes
		/// </summary>
		/// <returns>The entire StdOut generated from the process as a single trimmed string</returns>
		/// <param name="Command">Command to run</param>
		/// <param name="Args">Arguments to Command</param>
		/// <param name="Logger">Logger for output</param>
		private static string RunLocalProcessAndReturnStdOut(string Command, string Args, ILogger? Logger = null)
		{
			return RunLocalProcessAndReturnStdOut(Command, Args, out _, Logger);	
		}

		/// <summary>
		/// Runs a command line process, and returns simple StdOut output.
		/// </summary>
		/// <returns>The entire StdOut generated from the process as a single trimmed string</returns>
		/// <param name="Command">Command to run</param>
		/// <param name="Args">Arguments to Command</param>
		/// <param name="ExitCode">The return code from the process after it exits</param>
		/// <param name="Logger">Whether to also log standard output and standard error</param>
		private static string RunLocalProcessAndReturnStdOut(string Command, string? Args, out int ExitCode, ILogger? Logger = null)
		{
			// Process Arguments follow windows conventions in .NET Core
			// Which means single quotes ' are not considered quotes.
			// see https://github.com/dotnet/runtime/issues/29857
			// also see UE-102580
			// for rules see https://docs.microsoft.com/en-us/cpp/cpp/main-function-command-line-args
			Args = Args?.Replace('\'', '\"');

			ProcessStartInfo StartInfo = Args != null ? new ProcessStartInfo(Command, Args) : new ProcessStartInfo(Command);
			StartInfo.UseShellExecute = false;
			StartInfo.RedirectStandardInput = true;
			StartInfo.RedirectStandardOutput = true;
			StartInfo.RedirectStandardError = true;
			StartInfo.CreateNoWindow = true;
			StartInfo.StandardOutputEncoding = Encoding.UTF8;

			string FullOutput = "";
			string ErrorOutput = "";
			using (Process? LocalProcess = Process.Start(StartInfo))
			{
				if (LocalProcess != null)
				{
					StreamReader OutputReader = LocalProcess.StandardOutput;
					// trim off any extraneous new lines, helpful for those one-line outputs
					FullOutput = OutputReader.ReadToEnd().Trim();

					StreamReader ErrorReader = LocalProcess.StandardError;
					// trim off any extraneous new lines, helpful for those one-line outputs
					ErrorOutput = ErrorReader.ReadToEnd().Trim();
					if (Logger != null)
					{
						if (FullOutput.Length > 0)
						{
							Logger.LogInformation("{Message}", FullOutput);
						}

						if (ErrorOutput.Length > 0)
						{
							Logger.LogError("{Message}", ErrorOutput);
						}
					}

					LocalProcess.WaitForExit();
					ExitCode = LocalProcess.ExitCode;
				}
				else
				{
					ExitCode = -1; 
				}
			}

			// trim off any extraneous new lines, helpful for those one-line outputs
			if (ErrorOutput.Length > 0)
			{
				if (FullOutput.Length > 0)
				{
					FullOutput += Environment.NewLine;
				}
				FullOutput += ErrorOutput;
			}
			return FullOutput;
		}
	}
}