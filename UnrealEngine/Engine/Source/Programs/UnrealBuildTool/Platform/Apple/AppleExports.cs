// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Public Apple functions exposed to UAT
	/// </summary>
	public static class AppleExports
	{
		private static bool bForceModernXcode = Environment.CommandLine.Contains("-modernxcode", StringComparison.OrdinalIgnoreCase);
		private static bool bForceLegacyXcode = Environment.CommandLine.Contains("-legacyxcode", StringComparison.OrdinalIgnoreCase);

		/// <summary>
		/// Is the current project using modern xcode?
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <returns></returns>
		public static bool UseModernXcode(FileReference? ProjectFile)
		{
			if (bForceModernXcode)
			{
				if (bForceLegacyXcode)
				{
					throw new BuildException("Both -modernxcode and -legacyxcode were specified, please use one or the other.");
				}
				Log.TraceInformationOnce("Forcing MODERN XCODE with -modernxcode");
				return true;
			}
			if (bForceLegacyXcode)
			{
				Log.TraceInformationOnce("Forcing LEGACY XCODE with -legacyxcode");
				return false;
			}

			// Modern Xcode mode does this now
			bool bUseModernXcode = false;
			if (OperatingSystem.IsMacOS())
			{
				ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectFile?.Directory, UnrealTargetPlatform.Mac);
				Ini.TryGetValue("/Script/MacTargetPlatform.XcodeProjectSettings", "bUseModernXcode", out bUseModernXcode);
				Log.TraceInformationOnce("Choosing {0} XCODE based on .ini settings", bUseModernXcode ? "MODERN" : "LEGACY");
			}
			else
			{
				Log.TraceInformationOnce("Forcing LEGACY XCODE because host OS is not Mac");
			}
			return bUseModernXcode;
		}

		/// <summary>
		/// Get the given project's Swift settings
		/// </summary>
		/// <param name="ProjectFile"></param>
		/// <param name="Platform"></param>
		/// <param name="bUseSwiftUIMain"></param>
		/// <param name="bCreateBridgingHeader"></param>
		public static void GetSwiftIntegrationSettings(FileReference? ProjectFile, UnrealTargetPlatform Platform, out bool bUseSwiftUIMain, out bool bCreateBridgingHeader)
		{
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectFile?.Directory, Platform);
			Ini.TryGetValue("/Script/VisionOSRuntimeSettings.VisionOSRuntimeSettings", "bUseSwiftUIMain", out bUseSwiftUIMain);
			Ini.TryGetValue("/Script/VisionOSRuntimeSettings.VisionOSRuntimeSettings", "bCreateBridgingHeader", out bCreateBridgingHeader);
		}

		/// <summary>
		///  This is a FilePath from UE settings, like /Game/Foo/Bar.txt
		/// </summary>
		/// <param name="ProductDirectory">Directory to use for /Game paths</param>
		/// <param name="FilePath"></param>
		/// <returns></returns>
		public static FileReference ConvertFilePath(DirectoryReference? ProductDirectory, string FilePath)
		{
			// for FilePath params, pull the path out of the struct
			if (FilePath.StartsWith("(FilePath=", StringComparison.OrdinalIgnoreCase))
			{
				FilePath = ConfigHierarchy.GetStructEntry(FilePath, "FilePath", false)!;
			}

			if (FilePath.StartsWith("/Engine/", StringComparison.OrdinalIgnoreCase))
			{
				return FileReference.Combine(Unreal.EngineDirectory, FilePath.Substring(8));
			}
			else if (ProductDirectory != null && FilePath.StartsWith("/Game/", StringComparison.OrdinalIgnoreCase))
			{
				return FileReference.Combine(ProductDirectory, FilePath.Substring(6));
			}
			else if (FilePath.StartsWith("/", StringComparison.OrdinalIgnoreCase))
			{
				// Absolute path
				return new FileReference(FilePath);
			}
			else
			{
				// UE-193103, using the file selector in UE will set the path relative to /Engine/Binaries/Mac
				return FileReference.Combine(Unreal.EngineDirectory, "Binaries", "Mac", FilePath);
			}
		}

		/// <summary>
		/// Convert UnrealTargetPlatform to the platform that Xcode uses for -destination
		/// </summary>
		/// <param name="Platform"></param>
		/// <param name="Architectures">The architecture we are targeting (really, this is for Simulators)</param>
		/// <returns></returns>
		public static string GetDestinationPlatform(UnrealTargetPlatform Platform, UnrealArchitectures Architectures)
		{
			if (Platform == UnrealTargetPlatform.Mac)
			{
				return "macOS";
			}
			else if (Platform == UnrealTargetPlatform.IOS)
			{
				return Architectures.SingleArchitecture == UnrealArch.IOSSimulator ? "iOS Simulator" : "iOS";
			}
			else if (Platform == UnrealTargetPlatform.TVOS)
			{
				return Architectures.SingleArchitecture == UnrealArch.TVOSSimulator ? "tvOS Simulator" : "tvOS";
			}
			else if (Platform == UnrealTargetPlatform.VisionOS)
			{
				return Architectures.SingleArchitecture == UnrealArch.IOSSimulator ? "visionOS Simulator" : "visionOS";
			}

			throw new BuildException($"Unknown plaform {Platform}");
		}

		/// <summary>
		/// Different ways that xcodebuild is run, so the scripts can behave appropriately
		/// </summary>
		public enum XcodeBuildMode
		{
			/// <summary>
			/// This is when hitting Build from in Xcode
			/// </summary>
			Default = 0,
			/// <summary>
			/// This runs after building when building on commandline directly with UBT
			/// </summary>
			PostBuildSync = 1,
			/// <summary>
			/// This runs when making a fully made .app in the Staged directory
			/// </summary>
			Stage = 2,
			/// <summary>
			/// This runs when packaging a full made .app into Project/Binaries
			/// </summary>
			Package = 3,
			/// <summary>
			/// This runs when packaging a .xcarchive for distribution
			/// </summary>
			Distribute = 4,
		}

		/// <summary>
		/// Generates a stub xcode project for the given project/platform/target combo, then builds or archives it
		/// </summary>
		/// <param name="ProjectFile">Project to build</param>
		/// <param name="Platform">Platform to build</param>
		/// <param name="Architectures">The architecture we are targeting (really, this is for Simulator to pass in the -destination field)</param>
		/// <param name="Configuration">Configuration to build</param>
		/// <param name="TargetName">Target to build</param>
		/// <param name="BuildMode">Sets an envvar used inside the xcode project to control certain features</param>
		/// <param name="Logger"></param>
		/// <param name="ExtraOptions">Any extra options to pass to xcodebuild</param>
		/// <param name="bForceDummySigning">If true, force signing with the - identity</param>
		/// <returns>xcode's exit code</returns>
		public static int BuildWithStubXcodeProject(FileReference? ProjectFile, UnrealTargetPlatform Platform, UnrealArchitectures Architectures, UnrealTargetConfiguration Configuration,
			string TargetName, XcodeBuildMode BuildMode, ILogger Logger, string ExtraOptions = "", bool bForceDummySigning=false)
		{
			DirectoryReference? GeneratedProjectFile;
			// we don't use distro flag when making a modern project
			AppleExports.GenerateRunOnlyXcodeProject(ProjectFile, Platform, TargetName, bForDistribution: false, Logger, out GeneratedProjectFile);

			if (GeneratedProjectFile == null)
			{
				return 1;
			}

			if (bForceDummySigning)
			{
				ExtraOptions += " CODE_SIGN_STYLE=Manual CODE_SIGN_IDENTITY= PROVISIONING_PROFILE_SPECIFIER=";
			}
			else
			{
				// look for the special app store connect key information
				ConfigHierarchy SharedPlatformIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectFile?.Directory, UnrealTargetPlatform.Mac);
				bool bUseAutomaticCodeSigning;
				SharedPlatformIni.TryGetValue("/Script/MacTargetPlatform.XcodeProjectSettings", "bUseAutomaticCodeSigning", out bUseAutomaticCodeSigning);

				// disable automatic signing, if some extra options imply manual
				if (ExtraOptions.Contains("CODE_SIGN_IDENTITY"))
				{
					bUseAutomaticCodeSigning = false;
				}

				if (bUseAutomaticCodeSigning)
				{
					ExtraOptions += " -allowProvisioningUpdates";
	
					// handle AppStore Connect settings
					bool bUseAppStoreConnect;
					SharedPlatformIni.TryGetValue("/Script/MacTargetPlatform.XcodeProjectSettings", "bUseAppStoreConnect", out bUseAppStoreConnect);
					if (bUseAppStoreConnect)
					{
						string? IssuerID, KeyID, KeyPath;
						if (SharedPlatformIni.TryGetValue("/Script/MacTargetPlatform.XcodeProjectSettings", "AppStoreConnectIssuerID", out IssuerID) &&
							SharedPlatformIni.TryGetValue("/Script/MacTargetPlatform.XcodeProjectSettings", "AppStoreConnectKeyID", out KeyID) &&
							SharedPlatformIni.TryGetValue("/Script/MacTargetPlatform.XcodeProjectSettings", "AppStoreConnectKeyPath", out KeyPath))
						{
							FileReference KeyFile = ConvertFilePath(ProjectFile?.Directory, KeyPath);
							ExtraOptions += $" -authenticationKeyIssuerID {IssuerID}";
							ExtraOptions += $" -authenticationKeyID {KeyID}";
							ExtraOptions += $" -authenticationKeyPath \"{KeyFile}\"";
						}
					}
				}
			}

			// run xcodebuild on the generated project to make the .app
			string XcodeBuildAction = BuildMode == XcodeBuildMode.Distribute ? "archive" : "build";
			return AppleExports.FinalizeAppWithXcode(GeneratedProjectFile!, Platform, Architectures, TargetName, Configuration.ToString(), XcodeBuildAction, ExtraOptions + $" UE_XCODE_BUILD_MODE={BuildMode}", Logger);
		}

		/// <summary>
		/// Genearate an run-only Xcode project, that is not meant to be used for anything else besides code-signing/running/etc of the native .app bundle
		/// </summary>
		/// <param name="UProjectFile">Location of .uproject file (or null for the engine project</param>
		/// <param name="Platform">The platform to generate a project for</param>
		/// <param name="TargetName">The name of the target being built, so we can generate a more minimal project</param>
		/// <param name="bForDistribution">True if this is making a bild for uploading to app store</param>
		/// <param name="Logger">Logging object</param>
		/// <param name="GeneratedProjectFile">Returns the .xcworkspace that was made</param>
		public static void GenerateRunOnlyXcodeProject(FileReference? UProjectFile, UnrealTargetPlatform Platform, string TargetName, bool bForDistribution, ILogger Logger, out DirectoryReference? GeneratedProjectFile)
		{
			AppleToolChain.GenerateRunOnlyXcodeProject(UProjectFile, Platform, TargetName, bForDistribution, Logger, out GeneratedProjectFile);
		}

		/// <summary>
		/// Version of FinalizeAppWithXcode that is meant for modern xcode mode, where we assume all codesigning is setup already in the project, so nothing else is needed
		/// </summary>
		/// <param name="XcodeProject">The .xcworkspace file to build</param>
		/// <param name="Platform">THe platform to make the .app for</param>
		/// <param name="Architectures">The architecture we are targeting (really, this is for Simulator to pass in the -destination field)</param>
		/// <param name="SchemeName">The name of the scheme (basically the target on the .xcworkspace)</param>
		/// <param name="Configuration">Which configuration to make (Debug, etc)</param>
		/// <param name="Action">Action (build, archive, etc)</param>
		/// <param name="ExtraOptions">Extra options to pass to xcodebuild</param>
		/// <param name="Logger">Logging object</param>
		/// <returns>xcode's exit code</returns>
		public static int FinalizeAppWithXcode(DirectoryReference XcodeProject, UnrealTargetPlatform Platform, UnrealArchitectures Architectures, string SchemeName, string Configuration, string Action, string ExtraOptions, ILogger Logger)
		{
			return AppleToolChain.FinalizeAppWithXcode(XcodeProject, Platform, Architectures, SchemeName, Configuration, Action, ExtraOptions, Logger);
		}

		/// <summary>
		/// Pass along the call to UEBuildTarget.MakeBinaryFileName, but taking an extension instead of a binary type, since the type is hidden
		/// </summary>
		/// <param name="BinaryName"></param>
		/// <param name="Platform"></param>
		/// <param name="Configuration"></param>
		/// <param name="Architectures"></param>
		/// <param name="UndecoratedConfiguration"></param>
		/// <param name="Extension"></param>
		/// <returns></returns>
		public static string MakeBinaryFileName(string BinaryName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, UnrealArchitectures Architectures, UnrealTargetConfiguration UndecoratedConfiguration, string? Extension)
		{
			string StandardBinaryName = UEBuildTarget.MakeBinaryFileName(BinaryName, Platform, Configuration, Architectures, UndecoratedConfiguration, UEBuildBinaryType.Executable);
			return System.IO.Path.ChangeExtension(StandardBinaryName, Extension);
		}
	}
}
