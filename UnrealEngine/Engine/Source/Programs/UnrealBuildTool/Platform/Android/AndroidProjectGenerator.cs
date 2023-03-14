// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.IO;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Base class for platform-specific project generators
	/// </summary>
	class AndroidProjectGenerator : PlatformProjectGenerator
	{
		/// <summary>
		/// Whether Android Game Development Extension is installed in the system. See https://developer.android.com/games/agde for more details.
		/// May be disabled by using -noagde on commandline
		/// </summary>
		private bool AGDEInstalled = false;

		public AndroidProjectGenerator(CommandLineArguments Arguments, ILogger Logger)
			: base(Arguments, Logger)
		{
			AGDEInstalled = false;
			if (OperatingSystem.IsWindows() && !Arguments.HasOption("-noagde"))
			{
				AGDEInstalled = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(@"SOFTWARE\WOW6432Node\Google\AndroidGameDevelopmentExtension")?.ValueCount > 0;

				if (!AGDEInstalled)
				{
					try
					{
						string? programFiles86 = Environment.GetEnvironmentVariable("ProgramFiles(x86)");
						if (programFiles86 != null)
						{
							string vswhereExe = Path.Join(programFiles86, @"Microsoft Visual Studio\Installer\vswhere.exe");
							if (File.Exists(vswhereExe))
							{
								using (Process p = new Process())
								{
									ProcessStartInfo info = new ProcessStartInfo
									{
										FileName = vswhereExe,
										Arguments = @"-find Common7\IDE\Extensions\*\Google.VisualStudio.Android.dll",
										RedirectStandardOutput = true,
										UseShellExecute = false
									};
									p.StartInfo = info;
									p.Start();
									AGDEInstalled = p.StandardOutput.ReadToEnd().Contains("Google.VisualStudio.Android.dll");
								}
							}
						}
					}
					catch (Exception ex)
					{
						Logger.LogInformation("Failed to identify AGDE installation status: {Message}", ex.Message);
					}
				}
			}
		}

		/// <summary>
		/// Enumerate all the platforms that this generator supports
		/// </summary>
		public override IEnumerable<UnrealTargetPlatform> GetPlatforms()
		{
			yield return UnrealTargetPlatform.Android;
		}

		/// <summary>
		/// Whether this build platform has native support for VisualStudio
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <param name="ProjectFileFormat"></param>
		/// <returns>bool    true if native VisualStudio support (or custom VSI) is available</returns>
		public override bool HasVisualStudioSupport(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat ProjectFileFormat)
		{
			// Debugging, etc. are dependent on the TADP being installed
			return AGDEInstalled;
		}

		/// <summary>
		/// Return the VisualStudio platform name for this build platform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <returns>string    The name of the platform that VisualStudio recognizes</returns>
		public override string GetVisualStudioPlatformName(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			string PlatformName = InPlatform.ToString();

			if (InPlatform == UnrealTargetPlatform.Android && AGDEInstalled)
			{
				PlatformName = "Android-arm64-v8a";
			}

			return PlatformName;
		}

		/// <summary>
		/// Return any custom property group lines
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="ProjectFileFormat"></param>
		/// <param name="ProjectFileBuilder">String builder for the project file</param>
		public override void GetAdditionalVisualStudioPropertyGroups(UnrealTargetPlatform InPlatform, VCProjectFileFormat ProjectFileFormat, StringBuilder ProjectFileBuilder)
		{
			if (AGDEInstalled)
			{
				base.GetAdditionalVisualStudioPropertyGroups(InPlatform, ProjectFileFormat, ProjectFileBuilder);
			}
		}

		/// <summary>
		/// Return any custom property group lines
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="ProjectFileFormat"></param>
		/// <returns>string    The custom property import lines for the project file; Empty string if it doesn't require one</returns>
		public override string GetVisualStudioPlatformConfigurationType(UnrealTargetPlatform InPlatform, VCProjectFileFormat ProjectFileFormat)
		{
			string ConfigurationType = "";

			if (AGDEInstalled)
			{
				ConfigurationType = "Makefile";
			}
			else
			{
				ConfigurationType = base.GetVisualStudioPlatformConfigurationType(InPlatform, ProjectFileFormat);
			}

			return ConfigurationType;
		}

		/// <summary>
		/// Return the platform toolset string to write into the project configuration
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration"> The UnrealTargetConfiguration being built</param>
		/// <param name="InProjectFileFormat">The version of Visual Studio to target</param>
		/// <param name="ProjectFileBuilder">String builder for the project file</param>
		/// <returns>string    The custom configuration section for the project file; Empty string if it doesn't require one</returns>
		public override void GetVisualStudioPlatformToolsetString(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, VCProjectFileFormat InProjectFileFormat, StringBuilder ProjectFileBuilder)
		{
			VCProjectFileGenerator.AppendPlatformToolsetProperty(ProjectFileBuilder, InProjectFileFormat);
		}

		/// <summary>
		/// Return any custom paths for VisualStudio this platform requires
		/// This include ReferencePath, LibraryPath, LibraryWPath, IncludePath and ExecutablePath.
		/// </summary>
		/// <param name="InPlatform">The UnrealTargetPlatform being built</param>
		/// <param name="InConfiguration">The configuration being built</param>
		/// <param name="TargetType">The type of target (game or program)</param>
		/// <param name="TargetRulesPath">Path to the target.cs file</param>
		/// <param name="ProjectFilePath">Path to the project file</param>
		/// <param name="NMakeOutputPath"></param>
		/// <param name="InProjectFileFormat">Format for the generated project files</param>
		/// <param name="ProjectFileBuilder">String builder for the project file</param>
		/// <returns>The custom path lines for the project file; Empty string if it doesn't require one</returns>
		public override void GetVisualStudioPathsEntries(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration, TargetType TargetType, FileReference TargetRulesPath, FileReference ProjectFilePath, FileReference NMakeOutputPath, VCProjectFileFormat InProjectFileFormat, StringBuilder ProjectFileBuilder)
		{
			if (AGDEInstalled)
			{
				string apkLocation = Path.Combine(
					Path.GetDirectoryName(NMakeOutputPath.FullName)!,
					Path.GetFileNameWithoutExtension(NMakeOutputPath.FullName) + "-arm64.apk");

				ProjectFileBuilder.AppendLine($"    <AndroidApkLocation>{apkLocation}</AndroidApkLocation>");
				string intermediatePath = Path.GetFullPath(Path.GetDirectoryName(NMakeOutputPath.FullName) + @"\..\..\Intermediate\Android\arm64\");
				string symbolLocations = $@"{intermediatePath}jni\arm64-v8a;{intermediatePath}libs\arm64-v8a";
				ProjectFileBuilder.AppendLine($"    <AndroidSymbolDirectories>{symbolLocations}</AndroidSymbolDirectories>");
			}
			else
			{
				base.GetVisualStudioPathsEntries(InPlatform, InConfiguration, TargetType, TargetRulesPath, ProjectFilePath, NMakeOutputPath, InProjectFileFormat, ProjectFileBuilder);
			}
		}

		public override string GetExtraBuildArguments(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			// do not need to check InPlatform since it will always be UnrealTargetPlatform.Android
			return (AGDEInstalled ? " -Architectures=arm64 -ForceAPKGeneration" : "") + base.GetExtraBuildArguments(InPlatform, InConfiguration);
		}

		public override string GetVisualStudioUserFileStrings(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration,
			string InConditionString, TargetRules InTargetRules, FileReference TargetRulesPath, FileReference ProjectFilePath)
		{
			if (AGDEInstalled 
				&& (InPlatform == UnrealTargetPlatform.Android)
				&& ((InTargetRules.Type == TargetRules.TargetType.Client) || (InTargetRules.Type == TargetRules.TargetType.Game)))
			{
				string UserFileEntry = "<PropertyGroup " + InConditionString + ">\n";
				UserFileEntry		+= "	<AndroidLldbStartupCommands>" +
												"command script import \"" + Path.Combine(Unreal.EngineDirectory.FullName, "Extras", "LLDBDataFormatters", "UEDataFormatters_2ByteChars.py") + "\";" +
												"$(AndroidLldbStartupCommands)" +
											"</AndroidLldbStartupCommands>\n";
				UserFileEntry		+= "</PropertyGroup>\n";
				return UserFileEntry;
			}

			return base.GetVisualStudioUserFileStrings(InPlatform, InConfiguration, InConditionString, InTargetRules, TargetRulesPath, ProjectFilePath);
		}
	}
}
