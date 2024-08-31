// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Xml.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class UEDeployAndroid : UEBuildDeploy, IAndroidDeploy
	{
		private const string XML_HEADER = "<?xml version=\"1.0\" encoding=\"utf-8\"?>";

		// filename of current BundleTool
		private const string BUNDLETOOL_JAR = "bundletool-all-0.13.0.jar";

		// classpath of default android build tools gradle plugin
		private const string ANDROID_TOOLS_BUILD_GRADLE_VERSION = "com.android.tools.build:gradle:7.4.2";

		// default NDK version if not set
		private const string DEFAULT_NDK_VERSION = "25.1.8937393";

		// name of the only vulkan validation layer we're interested in 
		private const string ANDROID_VULKAN_VALIDATION_LAYER = "libVkLayer_khronos_validation.so";

		// Minimum Android SDK that must be used for Java compiling
		readonly int MinimumSDKLevel = 30;

		// Minimum SDK version needed for App Bundles
		readonly int MinimumSDKLevelForBundle = 21;

		// Minimum SDK version needed for Gradle based on active plugins
		private int MinimumSDKLevelForGradle = 19;

		// Reserved Java keywords not allowed in package names without modification
		private static string[] JavaReservedKeywords = new string[] {
			"abstract", "assert", "boolean", "break", "byte", "case", "catch", "char", "class", "const", "continue", "default", "do",
			"double", "else", "enum", "extends", "final", "finally", "float", "for", "goto", "if", "implements", "import", "instanceof",
			"int", "interface", "long", "native", "new", "package", "private", "protected", "public", "return", "short", "static",
			"strictfp", "super", "switch", "sychronized", "this", "throw", "throws", "transient", "try", "void", "volatile", "while",
			"false", "null", "true"
		};

		/// <summary>
		/// Internal usage for GetApiLevel
		/// </summary>
		private List<string>? PossibleApiLevels = null;

		protected FileReference? ProjectFile;

		/// <summary>
		/// Determines whether we package data inside the APK. Based on and  OR of "-ForcePackageData" being
		/// false and bPackageDataInsideApk in /Script/AndroidRuntimeSettings.AndroidRuntimeSettings being true
		/// </summary>
		protected bool bPackageDataInsideApk = false;

		/// <summary>
		/// Ignore AppBundle (AAB) generation setting if "-ForceAPKGeneration" specified
		/// </summary>
		[CommandLine("-ForceAPKGeneration", Value = "true")]
		public bool ForceAPKGeneration = false;

		/// <summary>
		/// Do not use Gradle if previous APK exists and only libUnreal.so changed
		/// </summary>
		[CommandLine("-BypassGradlePackaging", Value = "true")]
		public bool BypassGradlePackaging = false;

		/// <summary>
		/// Whether UBT is invoked from MSBuild.
		/// If false will, disable bDontBundleLibrariesInAPK, unless forced .
		/// </summary>
		[CommandLine(Prefix = "-FromMsBuild", Value = "true")]
		public bool bFromMSBuild = false;

		/// <summary>
		/// Forcing bDontBundleLibrariesInAPK to "true" or "false" ignoring any other option.
		/// </summary>
		[CommandLine("-ForceDontBundleLibrariesInAPK=")]
		public bool? ForceDontBundleLibrariesInAPK;

		/// <summary>
		/// Adds LD_PRELOAD'ed .so to capture all malloc/free/etc calls to libc.so and route them to our memory tracing.
		/// </summary>
		[CommandLine("-ScudoMemoryTracing", Value = "true")]
		public bool bEnableScudoMemoryTracing = false;

		public UEDeployAndroid(FileReference? InProjectFile, bool InForcePackageData, ILogger InLogger)
			: base(InLogger)
		{
			ProjectFile = InProjectFile;

			// read the ini value and OR with the command line value
			bool IniValue = ReadPackageDataInsideApkFromIni(null);
			bPackageDataInsideApk = InForcePackageData || IniValue == true;

			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), this, Logger);
		}

		private UnrealPluginLanguage? UPL = null;
		private string ActiveUPLFiles = "";
		private string? UPLHashCode = null;
		private bool ARCorePluginEnabled = false;
		private bool FacebookPluginEnabled = false;
		private bool OculusMobilePluginEnabled = false;
		private bool EOSSDKPluginEnabled = false;
		private UnrealArchitectures? Architectures = null;

		public void SetAndroidPluginData(UnrealArchitectures Architectures, List<string> inPluginExtraData)
		{
			this.Architectures = Architectures;

			List<string> NDKArches = Architectures.Architectures.Select(x => GetNDKArch(x)).ToList();

			// check if certain plugins are enabled
			ARCorePluginEnabled = false;
			FacebookPluginEnabled = false;
			OculusMobilePluginEnabled = false;
			EOSSDKPluginEnabled = false;
			ActiveUPLFiles = "";
			foreach (string Plugin in inPluginExtraData)
			{
				ActiveUPLFiles += Plugin + "\n";

				// check if the Facebook plugin was enabled
				if (Plugin.Contains("OnlineSubsystemFacebook_UPL"))
				{
					FacebookPluginEnabled = true;
					continue;
				}

				// check if the ARCore plugin was enabled
				if (Plugin.Contains("GoogleARCoreBase_APL"))
				{
					ARCorePluginEnabled = true;
					continue;
				}

				// check if the Oculus Mobile plugin was enabled
				if (Plugin.Contains("OculusMobile_APL"))
				{
					OculusMobilePluginEnabled = true;
					continue;
				}

				// check if the EOSShared plugin was enabled
				if (Plugin.Contains("EOSSDK"))
				{
					EOSSDKPluginEnabled = true;
					continue;
				}
			}

			UPL = new UnrealPluginLanguage(ProjectFile, inPluginExtraData, NDKArches, "http://schemas.android.com/apk/res/android", "xmlns:android=\"http://schemas.android.com/apk/res/android\" xmlns:tools=\"http://schemas.android.com/tools\"", UnrealTargetPlatform.Android, Logger);
			UPLHashCode = UPL.GetUPLHash();
			//			APL.SetTrace();
		}

		private void SetMinimumSDKLevelForGradle()
		{
			if (FacebookPluginEnabled)
			{
				MinimumSDKLevelForGradle = Math.Max(MinimumSDKLevelForGradle, 15);
			}
			if (ARCorePluginEnabled)
			{
				MinimumSDKLevelForGradle = Math.Max(MinimumSDKLevelForGradle, 19);
			}
			if (EOSSDKPluginEnabled)
			{
				MinimumSDKLevelForGradle = Math.Max(MinimumSDKLevelForGradle, 23);
			}
		}

		/// <summary>
		/// Simple function to pipe output asynchronously
		/// </summary>
		private void ParseApiLevel(object Sender, DataReceivedEventArgs Event)
		{
			// DataReceivedEventHandler is fired with a null string when the output stream is closed.  We don't want to
			// print anything for that event.
			if (!String.IsNullOrEmpty(Event.Data))
			{
				string Line = Event.Data;
				if (Line.StartsWith("id:"))
				{
					// the line should look like: id: 1 or "android-19"
					string[] Tokens = Line.Split("\"".ToCharArray());
					if (Tokens.Length >= 2)
					{
						PossibleApiLevels!.Add(Tokens[1]);
					}
				}
			}
		}

		private ConfigHierarchy GetConfigCacheIni(ConfigHierarchyType Type)
		{
			return ConfigCache.ReadHierarchy(Type, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
		}

		private bool ValidateSDK(string PlatformsDir, string ApiString)
		{
			if (!Directory.Exists(PlatformsDir))
			{
				return false;
			}

			string SDKPlatformDir = Path.Combine(PlatformsDir, ApiString);
			return Directory.Exists(SDKPlatformDir);
		}

		private int GetApiLevelInt(string ApiString)
		{
			int VersionInt = 0;
			if (ApiString.Contains('-'))
			{
				int Version;
				if (Int32.TryParse(ApiString.Substring(ApiString.LastIndexOf('-') + 1), out Version))
				{
					VersionInt = Version;
				}
			}
			return VersionInt;
		}

		private string? CachedSDKLevel = null;
		private string GetSdkApiLevel(AndroidToolChain ToolChain)
		{
			if (CachedSDKLevel == null)
			{
				// ask the .ini system for what version to use
				ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
				string SDKLevel;
				Ini.GetString("/Script/AndroidPlatformEditor.AndroidSDKSettings", "SDKAPILevel", out SDKLevel);

				// check for project override of SDK API level
				string ProjectSDKLevel;
				Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "SDKAPILevelOverride", out ProjectSDKLevel);
				ProjectSDKLevel = ProjectSDKLevel.Trim();
				if (!String.IsNullOrEmpty(ProjectSDKLevel))
				{
					SDKLevel = ProjectSDKLevel;
				}

				// if we want to use whatever version the ndk uses, then use that
				if (SDKLevel == "matchndk")
				{
					SDKLevel = ToolChain.GetNdkApiLevel();
				}

				// run a command and capture output
				if (SDKLevel == "latest")
				{
					SDKLevel = ToolChain.GetLargestApiLevel();
				}

				// make sure it is at least android-23
				int SDKLevelInt = GetApiLevelInt(SDKLevel);
				if (SDKLevelInt < MinimumSDKLevel)
				{
					Logger.LogInformation("Requires at least SDK API level {MinimumSDKLevel}, currently set to '{SDKLevel}'", MinimumSDKLevel, SDKLevel);
					SDKLevel = ToolChain.GetLargestApiLevel();

					SDKLevelInt = GetApiLevelInt(SDKLevel);
					if (SDKLevelInt < MinimumSDKLevel)
					{
						SDKLevelInt = MinimumSDKLevel;
						SDKLevel = "android-" + MinimumSDKLevel.ToString();
						Logger.LogInformation("Gradle will attempt to download SDK API level {SDKLevelInt}", SDKLevelInt);
					}
				}

				// validate the platform SDK is installed
				string PlatformsDir = Path.Combine(Environment.ExpandEnvironmentVariables("%ANDROID_HOME%"), "platforms");
				if (!ValidateSDK(PlatformsDir, SDKLevel))
				{
					Logger.LogWarning("The SDK API requested '{SdkLevel}' not installed in {PlatformsDir}; Gradle will attempt to download it.", SDKLevel, PlatformsDir);
				}

				Logger.LogInformation("Building Java with SDK API level '{SDKLevel}'", SDKLevel);
				CachedSDKLevel = SDKLevel;
			}

			return CachedSDKLevel;
		}

		private string? CachedBuildToolsVersion = null;
		private string? LastAndroidHomePath = null;

		private uint GetRevisionValue(string VersionString)
		{
			if (VersionString == null)
			{
				return 0;
			}

			// read up to 4 sections (ie. 20.0.3.5), first section most significant
			// each section assumed to be 0 to 255 range
			uint Value = 0;
			try
			{
				string[] Sections = VersionString.Split(".".ToCharArray());
				Value |= (Sections.Length > 0) ? (UInt32.Parse(Sections[0]) << 24) : 0;
				Value |= (Sections.Length > 1) ? (UInt32.Parse(Sections[1]) << 16) : 0;
				Value |= (Sections.Length > 2) ? (UInt32.Parse(Sections[2]) << 8) : 0;
				Value |= (Sections.Length > 3) ? UInt32.Parse(Sections[3]) : 0;
			}
			catch (Exception)
			{
				// ignore poorly formed version
			}
			return Value;
		}

		private string GetBuildToolsVersion()
		{
			// return cached path if ANDROID_HOME has not changed
			string HomePath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%");
			if (CachedBuildToolsVersion != null && LastAndroidHomePath == HomePath)
			{
				return CachedBuildToolsVersion;
			}

			string? BestVersionString = null;
			uint BestVersion = 0;

			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "BuildToolsOverride", out BestVersionString);

			if (BestVersionString == null || String.IsNullOrEmpty(BestVersionString) || BestVersionString == "latest")
			{
				// get a list of the directories in build-tools.. may be more than one set installed (or none which is bad)
				string[] Subdirs = Directory.GetDirectories(Path.Combine(HomePath, "build-tools"));
				if (Subdirs.Length == 0)
				{
					throw new BuildException("Failed to find %ANDROID_HOME%/build-tools subdirectory. Run SDK manager and install build-tools.");
				}

				// valid directories will have a source.properties with the Pkg.Revision (there is no guarantee we can use the directory name as revision)
				foreach (string CandidateDir in Subdirs)
				{
					string AaptFilename = Path.Combine(CandidateDir, RuntimePlatform.IsWindows ? "aapt.exe" : "aapt");
					string RevisionString = "";
					uint RevisionValue = 0;

					if (File.Exists(AaptFilename))
					{
						string SourcePropFilename = Path.Combine(CandidateDir, "source.properties");
						if (File.Exists(SourcePropFilename))
						{
							string[] PropertyContents = File.ReadAllLines(SourcePropFilename);
							foreach (string PropertyLine in PropertyContents)
							{
								if (PropertyLine.StartsWith("Pkg.Revision="))
								{
									RevisionString = PropertyLine.Substring(13);
									RevisionValue = GetRevisionValue(RevisionString);
									break;
								}
							}
						}
					}

					// remember it if newer version or haven't found one yet
					if (RevisionValue > BestVersion || BestVersionString == null)
					{
						BestVersion = RevisionValue;
						BestVersionString = RevisionString;
					}
				}
			}

			if (BestVersionString == null)
			{
				BestVersionString = "33.0.1";
				Logger.LogWarning("Failed to find %ANDROID_HOME%/build-tools subdirectory. Will attempt to use {BestVersionString}.", BestVersionString);
			}

			BestVersion = GetRevisionValue(BestVersionString);

			// with Gradle enabled use at least 28.0.3 (will be installed by Gradle if missing)
			if (BestVersion < ((28 << 24) | (0 << 16) | (3 << 8)))
			{
				BestVersionString = "28.0.3";
			}

			// don't allow higher than 30.0.3 for now (will be installed by Gradle if missing)
			//			if (BestVersion > ((30 << 24) | (0 << 16) | (3 << 8)))
			//			{
			//				BestVersionString = "30.0.3";
			//			}

			CachedBuildToolsVersion = BestVersionString;
			LastAndroidHomePath = HomePath;

			Logger.LogInformation("Building with Build Tools version '{CachedBuildToolsVersion}'", CachedBuildToolsVersion);

			return CachedBuildToolsVersion;
		}

		public static string GetOBBVersionNumber(int PackageVersion)
		{
			string VersionString = PackageVersion.ToString("0");
			return VersionString;
		}

		public bool GetPackageDataInsideApk()
		{
			return bPackageDataInsideApk;
		}

		/// <summary>
		/// Reads the bPackageDataInsideApk from AndroidRuntimeSettings
		/// </summary>
		/// <param name="Ini"></param>
		protected bool ReadPackageDataInsideApkFromIni(ConfigHierarchy? Ini)
		{
			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			}

			// we check this a lot, so make it easy 
			bool bIniPackageDataInsideApk;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPackageDataInsideApk", out bIniPackageDataInsideApk);

			return bIniPackageDataInsideApk;
		}

		public bool UseExternalFilesDir(bool bDisallowExternalFilesDir, ConfigHierarchy? Ini = null)
		{
			if (bDisallowExternalFilesDir)
			{
				return false;
			}

			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			}

			// we check this a lot, so make it easy 
			bool bUseExternalFilesDir;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bUseExternalFilesDir", out bUseExternalFilesDir);

			return bUseExternalFilesDir;
		}

		public List<string> GetTargetOculusMobileDevices(ConfigHierarchy Ini)
		{
			// always false if the Oculus Mobile plugin wasn't enabled
			if (!OculusMobilePluginEnabled)
			{
				return new List<string>();
			}

			List<string>? OculusMobileDevices;
			bool result = Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "PackageForOculusMobile", out OculusMobileDevices);
			if (!result || OculusMobileDevices == null)
			{
				OculusMobileDevices = new List<string>();
			}

			return OculusMobileDevices;
		}

		public bool IsPackagingForMetaQuest(ConfigHierarchy? Ini = null)
		{
			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			}

			List<string> TargetOculusDevices = GetTargetOculusMobileDevices(Ini); // Backcompat for deprecated oculus device target setting
			bool bTargetOculusDevices = (TargetOculusDevices != null && TargetOculusDevices.Count() > 0); // Backcompat for deprecated oculus device target setting

			bool result = Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPackageForMetaQuest", out var bPackageForMetaQuest);

			return (result && bPackageForMetaQuest) || bTargetOculusDevices;
		}

		public bool DisableVerifyOBBOnStartUp(ConfigHierarchy? Ini = null)
		{
			// make a new one if one wasn't passed in
			if (Ini == null)
			{
				Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			}

			// we check this a lot, so make it easy 
			bool bDisableVerifyOBBOnStartUp;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bDisableVerifyOBBOnStartUp", out bDisableVerifyOBBOnStartUp);

			return bDisableVerifyOBBOnStartUp;
		}

		private static bool SafeDeleteFile(string Filename, bool bCheckExists = true)
		{
			if (!bCheckExists || File.Exists(Filename))
			{
				try
				{
					File.SetAttributes(Filename, FileAttributes.Normal);
					File.Delete(Filename);
					return true;
				}
				catch (System.UnauthorizedAccessException)
				{
					throw new BuildException("File '{0}' is in use; unable to modify it.", Filename);
				}
				catch (System.Exception)
				{
					return false;
				}
			}
			return true;
		}

		private static void CopyFileDirectory(string SourceDir, string DestDir, Dictionary<string, string>? Replacements = null, string[]? Excludes = null)
		{
			if (!Directory.Exists(SourceDir))
			{
				return;
			}

			string[] Files = Directory.GetFiles(SourceDir, "*.*", SearchOption.AllDirectories);
			foreach (string Filename in Files)
			{
				if (Excludes != null)
				{
					// skip files in excluded directories
					string DirectoryName = Path.GetFileName(Path.GetDirectoryName(Filename))!;
					bool bExclude = false;
					foreach (string Exclude in Excludes)
					{
						if (DirectoryName == Exclude)
						{
							bExclude = true;
							break;
						}
					}
					if (bExclude)
					{
						continue;
					}
				}

				// skip template files
				if (Path.GetExtension(Filename) == ".template")
				{
					continue;
				}

				// make the dst filename with the same structure as it was in SourceDir
				string DestFilename = Path.Combine(DestDir, Utils.MakePathRelativeTo(Filename, SourceDir)).Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);

				// make the subdirectory if needed
				string DestSubdir = Path.GetDirectoryName(DestFilename)!;
				if (!Directory.Exists(DestSubdir))
				{
					Directory.CreateDirectory(DestSubdir);
				}

				// some files are handled specially
				string Ext = Path.GetExtension(Filename);
				if (Ext == ".xml" && Replacements != null)
				{
					string Contents = File.ReadAllText(Filename);

					// replace some variables
					foreach (KeyValuePair<string, string> Pair in Replacements)
					{
						Contents = Contents.Replace(Pair.Key, Pair.Value);
					}

					bool bWriteFile = true;
					if (File.Exists(DestFilename))
					{
						string OriginalContents = File.ReadAllText(DestFilename);
						if (Contents == OriginalContents)
						{
							bWriteFile = false;
						}
					}

					// write out file if different
					if (bWriteFile)
					{
						SafeDeleteFile(DestFilename);
						File.WriteAllText(DestFilename, Contents);
					}
				}
				else
				{
					SafeDeleteFile(DestFilename);
					File.Copy(Filename, DestFilename);

					// preserve timestamp and clear read-only flags
					FileInfo DestFileInfo = new FileInfo(DestFilename);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
					File.SetLastWriteTimeUtc(DestFilename, File.GetLastWriteTimeUtc(Filename));
				}
			}
		}

		private static void DeleteDirectory(string InPath, ILogger Logger, string SubDirectoryToKeep = "")
		{
			// skip the dir we want to
			if (String.Compare(Path.GetFileName(InPath), SubDirectoryToKeep, true) == 0)
			{
				return;
			}

			// delete all files in here
			string[] Files;
			try
			{
				Files = Directory.GetFiles(InPath);
			}
			catch (Exception)
			{
				// directory doesn't exist so all is good
				return;
			}
			foreach (string Filename in Files)
			{
				try
				{
					// remove any read only flags
					FileInfo FileInfo = new FileInfo(Filename);
					FileInfo.Attributes = FileInfo.Attributes & ~FileAttributes.ReadOnly;
					FileInfo.Delete();
				}
				catch (Exception)
				{
					Logger.LogInformation("Failed to delete all files in directory {InPath}. Continuing on...", InPath);
				}
			}

			string[] Dirs = Directory.GetDirectories(InPath, "*.*", SearchOption.TopDirectoryOnly);
			foreach (string Dir in Dirs)
			{
				DeleteDirectory(Dir, Logger, SubDirectoryToKeep);
				// try to delete the directory, but allow it to fail (due to SubDirectoryToKeep still existing)
				try
				{
					Directory.Delete(Dir);
				}
				catch (Exception)
				{
					// do nothing
				}
			}
		}

		private bool BinaryFileEquals(string SourceFilename, string DestFilename)
		{
			if (!File.Exists(SourceFilename))
			{
				return false;
			}
			if (!File.Exists(DestFilename))
			{
				return false;
			}

			FileInfo SourceInfo = new FileInfo(SourceFilename);
			FileInfo DestInfo = new FileInfo(DestFilename);
			if (SourceInfo.Length != DestInfo.Length)
			{
				return false;
			}

			using (FileStream SourceStream = new FileStream(SourceFilename, FileMode.Open, FileAccess.Read, FileShare.Read))
			using (BinaryReader SourceReader = new BinaryReader(SourceStream))
			using (FileStream DestStream = new FileStream(DestFilename, FileMode.Open, FileAccess.Read, FileShare.Read))
			using (BinaryReader DestReader = new BinaryReader(DestStream))
			{
				while (true)
				{
					byte[] SourceData = SourceReader.ReadBytes(4096);
					byte[] DestData = DestReader.ReadBytes(4096);
					if (SourceData.Length != DestData.Length)
					{
						return false;
					}
					if (SourceData.Length == 0)
					{
						return true;
					}
					if (!SourceData.SequenceEqual(DestData))
					{
						return false;
					}
				}
			}
		}

		private bool CopyIfDifferent(string SourceFilename, string DestFilename, bool bLog, bool bContentCompare)
		{
			if (!File.Exists(SourceFilename))
			{
				return false;
			}

			bool bDestFileAlreadyExists = File.Exists(DestFilename);
			bool bNeedCopy = !bDestFileAlreadyExists;

			if (!bNeedCopy)
			{
				if (bContentCompare)
				{
					bNeedCopy = !BinaryFileEquals(SourceFilename, DestFilename);
				}
				else
				{
					FileInfo SourceInfo = new FileInfo(SourceFilename);
					FileInfo DestInfo = new FileInfo(DestFilename);

					if (SourceInfo.Length != DestInfo.Length)
					{
						bNeedCopy = true;
					}
					else if (File.GetLastWriteTimeUtc(DestFilename) < File.GetLastWriteTimeUtc(SourceFilename))
					{
						// destination file older than source
						bNeedCopy = true;
					}
				}
			}

			if (bNeedCopy)
			{
				if (bLog)
				{
					Logger.LogInformation("Copying {SourceFilename} to {DestFilename}", SourceFilename, DestFilename);
				}

				if (bDestFileAlreadyExists)
				{
					SafeDeleteFile(DestFilename, false);
				}
				File.Copy(SourceFilename, DestFilename);
				File.SetLastWriteTimeUtc(DestFilename, File.GetLastWriteTimeUtc(SourceFilename));

				// did copy
				return true;
			}

			// did not copy
			return false;
		}

		private void CleanCopyDirectory(string SourceDir, string DestDir, string[]? Excludes = null)
		{
			if (!Directory.Exists(SourceDir))
			{
				return;
			}
			if (!Directory.Exists(DestDir))
			{
				CopyFileDirectory(SourceDir, DestDir, null, Excludes);
				return;
			}

			// copy files that are different and make a list of ones to keep
			string[] StartingSourceFiles = Directory.GetFiles(SourceDir, "*.*", SearchOption.AllDirectories);
			List<string> FilesToKeep = new List<string>();
			foreach (string Filename in StartingSourceFiles)
			{
				if (Excludes != null)
				{
					// skip files in excluded directories
					string DirectoryName = Path.GetFileName(Path.GetDirectoryName(Filename))!;
					bool bExclude = false;
					foreach (string Exclude in Excludes)
					{
						if (DirectoryName == Exclude)
						{
							bExclude = true;
							break;
						}
					}
					if (bExclude)
					{
						continue;
					}
				}

				// make the dest filename with the same structure as it was in SourceDir
				string DestFilename = Path.Combine(DestDir, Utils.MakePathRelativeTo(Filename, SourceDir));

				// remember this file to keep
				FilesToKeep.Add(DestFilename);

				// only copy files that are new or different
				if (FilesAreDifferent(Filename, DestFilename))
				{
					if (File.Exists(DestFilename))
					{
						// xml files may have been rewritten but contents still the same so check contents also
						string Ext = Path.GetExtension(Filename);
						if (Ext == ".xml")
						{
							if (File.ReadAllText(Filename) == File.ReadAllText(DestFilename))
							{
								continue;
							}
						}

						// delete it so can copy over it
						SafeDeleteFile(DestFilename);
					}

					// make the subdirectory if needed
					string DestSubdir = Path.GetDirectoryName(DestFilename)!;
					if (!Directory.Exists(DestSubdir))
					{
						Directory.CreateDirectory(DestSubdir);
					}

					// copy it
					File.Copy(Filename, DestFilename);

					// preserve timestamp and clear read-only flags
					FileInfo DestFileInfo = new FileInfo(DestFilename);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
					File.SetLastWriteTimeUtc(DestFilename, File.GetLastWriteTimeUtc(Filename));

					Logger.LogInformation("Copied file {DestFilename}.", DestFilename);
				}
			}

			// delete any files not in the keep list
			string[] StartingDestFiles = Directory.GetFiles(DestDir, "*.*", SearchOption.AllDirectories);
			foreach (string Filename in StartingDestFiles)
			{
				if (!FilesToKeep.Contains(Filename))
				{
					Logger.LogInformation("Deleting unneeded file {Filename}.", Filename);
					SafeDeleteFile(Filename);
				}
			}

			// delete any empty directories
			try
			{
				IEnumerable<string> BaseDirectories = Directory.EnumerateDirectories(DestDir, "*", SearchOption.AllDirectories).OrderByDescending(x => x);
				foreach (string directory in BaseDirectories)
				{
					if (Directory.Exists(directory) && Directory.GetFiles(directory, "*.*", SearchOption.AllDirectories).Count() == 0)
					{
						Logger.LogInformation("Cleaning Directory {Directory} as empty.", directory);
						Directory.Delete(directory, true);
					}
				}
			}
			catch (Exception)
			{
				// likely System.IO.DirectoryNotFoundException, ignore it
			}
		}

		public string GetUnrealBuildFilePath(String EngineDirectory)
		{
			return Path.GetFullPath(Path.Combine(EngineDirectory, "Build/Android/Java"));
		}

		public string GetUnrealPreBuiltFilePath(String EngineDirectory)
		{
			return Path.GetFullPath(Path.Combine(EngineDirectory, "Build/Android/Prebuilt"));
		}

		public string GetUnrealJavaSrcPath()
		{
			return Path.Combine("src", "com", "epicgames", "unreal");
		}

		public string GetUnrealJavaFilePath(String EngineDirectory)
		{
			return Path.GetFullPath(Path.Combine(GetUnrealBuildFilePath(EngineDirectory), GetUnrealJavaSrcPath()));
		}

		public string GetUnrealJavaBuildSettingsFileName(String EngineDirectory)
		{
			return Path.Combine(GetUnrealJavaFilePath(EngineDirectory), "JavaBuildSettings.java");
		}

		public string GetUnrealJavaDownloadShimFileName(string Directory)
		{
			return Path.Combine(Directory, "DownloadShim.java");
		}

		public string GetUnrealTemplateJavaSourceDir(string Directory)
		{
			return Path.Combine(GetUnrealBuildFilePath(Directory), "JavaTemplates");
		}

		public string GetUnrealTemplateJavaDestination(string Directory, string FileName)
		{
			return Path.Combine(Directory, FileName);
		}

		public string GetUnrealJavaOBBDataFileName(string Directory)
		{
			return Path.Combine(Directory, "OBBData.java");
		}

		public class TemplateFile
		{
			public string SourceFile;
			public string DestinationFile;

			public TemplateFile(string SourceFile, string DestinationFile)
			{
				this.SourceFile = SourceFile;
				this.DestinationFile = DestinationFile;
			}
		}

		private void MakeDirectoryIfRequired(string DestFilename)
		{
			string DestSubdir = Path.GetDirectoryName(DestFilename)!;
			if (!Directory.Exists(DestSubdir))
			{
				Directory.CreateDirectory(DestSubdir);
			}
		}

		private int CachedStoreVersion = -1;
		private int CachedStoreVersionOffsetArm64 = 0;
		private int CachedStoreVersionOffsetX8664 = 0;

		public int GetStoreVersion(UnrealArch? Architecture)
		{
			if (CachedStoreVersion < 1)
			{
				ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
				int StoreVersion = 1;
				Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersion", out StoreVersion);

				bool bUseChangeListAsStoreVersion = false;
				Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bUseChangeListAsStoreVersion", out bUseChangeListAsStoreVersion);

				bool IsBuildMachine = Unreal.IsBuildMachine();
				// override store version with changelist if enabled and is build machine
				if (bUseChangeListAsStoreVersion && IsBuildMachine)
				{
					// make sure changelist is cached (clear unused warning)
					string EngineVersion = ReadEngineVersion();
					if (EngineVersion == null)
					{
						throw new BuildException("No engine version!");
					}

					int Changelist = 0;
					if (Int32.TryParse(EngineChangelist, out Changelist))
					{
						if (Changelist != 0)
						{
							StoreVersion = Changelist;
						}
					}
				}

				Logger.LogInformation("GotStoreVersion found v{StoreVersion}. (bUseChangeListAsStoreVersion={bUseChangeListAsStoreVersion} IsBuildMachine={IsBuildMachine} EngineChangeList={EngineChangeList})", StoreVersion, bUseChangeListAsStoreVersion, IsBuildMachine, EngineChangelist);

				CachedStoreVersion = StoreVersion;

				Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersionOffsetArm64", out CachedStoreVersionOffsetArm64);
				Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "StoreVersionOffsetX8664", out CachedStoreVersionOffsetX8664);
			}

			if (Architecture == UnrealArch.Arm64)
			{
				return CachedStoreVersion + CachedStoreVersionOffsetArm64;
			}
			else if (Architecture == UnrealArch.X64)
			{
				return CachedStoreVersion + CachedStoreVersionOffsetX8664;
			}

			return CachedStoreVersion;
		}

		private string? CachedVersionDisplayName = null;

		public string GetVersionDisplayName(bool bIsEmbedded)
		{
			if (String.IsNullOrEmpty(CachedVersionDisplayName))
			{
				ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
				string VersionDisplayName = "";
				Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "VersionDisplayName", out VersionDisplayName);

				if (Unreal.IsBuildMachine())
				{
					bool bAppendChangeListToVersionDisplayName = false;
					Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bAppendChangeListToVersionDisplayName", out bAppendChangeListToVersionDisplayName);
					if (bAppendChangeListToVersionDisplayName)
					{
						VersionDisplayName = String.Format("{0}-{1}", VersionDisplayName, EngineChangelist);
					}

					bool bAppendPlatformToVersionDisplayName = false;
					Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bAppendPlatformToVersionDisplayName", out bAppendPlatformToVersionDisplayName);
					if (bAppendPlatformToVersionDisplayName)
					{
						VersionDisplayName = String.Format("{0}-Android", VersionDisplayName);
					}

					// append optional text to version name if embedded build
					if (bIsEmbedded)
					{
						string EmbeddedAppendDisplayName = "";
						if (Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "EmbeddedAppendDisplayName", out EmbeddedAppendDisplayName))
						{
							VersionDisplayName = VersionDisplayName + EmbeddedAppendDisplayName;
						}
					}
				}

				CachedVersionDisplayName = VersionDisplayName;
			}

			return CachedVersionDisplayName;
		}

		public void WriteJavaOBBDataFile(string FileName, string PackageName, List<string> ObbSources, string CookFlavor, bool bPackageDataInsideApk, UnrealArch UnrealArch)
		{
			Logger.LogInformation("\n==== Writing to OBB data file {FileName} ====", FileName);

			// always must write if file does not exist
			bool bFileExists = File.Exists(FileName);
			bool bMustWriteFile = !bFileExists;

			string AppType = "";
			if (CookFlavor.EndsWith("Client"))
			{
				//				AppType = ".Client";		// should always be empty now; fix up the name in batch file instead
			}

			int StoreVersion = GetStoreVersion(UnrealArch);

			StringBuilder obbData = new StringBuilder("package " + PackageName + ";\n\n");
			obbData.Append("public class OBBData\n{\n");
			obbData.Append("public static final String AppType = \"" + AppType + "\";\n\n");
			obbData.Append("public static class XAPKFile {\npublic final boolean mIsMain;\npublic final String mFileVersion;\n");
			obbData.Append("public final long mFileSize;\nXAPKFile(boolean isMain, String fileVersion, long fileSize) {\nmIsMain = isMain;\nmFileVersion = fileVersion;\nmFileSize = fileSize;\n");
			obbData.Append("}\n}\n\n");

			// write the data here
			obbData.Append("public static final XAPKFile[] xAPKS = {\n");
			// For each obb file... but we only have one... for now anyway.
			bool first = ObbSources.Count > 1;
			bool AnyOBBExists = false;
			foreach (string ObbSource in ObbSources)
			{
				bool bOBBExists = File.Exists(ObbSource);
				AnyOBBExists |= bOBBExists;

				obbData.Append("new XAPKFile(\n" + (ObbSource.Contains(".patch.") ? "false, // false signifies a patch file\n" : "true, // true signifies a main file\n"));
				obbData.AppendFormat("\"{0}\", // the version of the APK that the file was uploaded against\n", GetOBBVersionNumber(StoreVersion));
				obbData.AppendFormat("{0}L // the length of the file in bytes\n", bOBBExists ? new FileInfo(ObbSource).Length : 0);
				obbData.AppendFormat("){0}\n", first ? "," : "");
				first = false;
			}
			obbData.Append("};\n"); // close off data
			obbData.Append("};\n"); // close class definition off

			// see if we need to replace the file if it exists
			if (!bMustWriteFile && bFileExists)
			{
				string[] obbDataFile = File.ReadAllLines(FileName);

				// Must always write if AppType not defined
				bool bHasAppType = false;
				foreach (string FileLine in obbDataFile)
				{
					if (FileLine.Contains("AppType ="))
					{
						bHasAppType = true;
						break;
					}
				}
				if (!bHasAppType)
				{
					bMustWriteFile = true;
				}

				// OBB must exist, contents must be different, and not packaging in APK to require replacing
				if (!bMustWriteFile && AnyOBBExists && !bPackageDataInsideApk && !obbDataFile.SequenceEqual((obbData.ToString()).Split('\n')))
				{
					bMustWriteFile = true;
				}
			}

			if (bMustWriteFile)
			{
				MakeDirectoryIfRequired(FileName);
				using (StreamWriter outputFile = new StreamWriter(FileName, false))
				{
					string[] obbSrc = obbData.ToString().Split('\n');
					foreach (string line in obbSrc)
					{
						outputFile.WriteLine(line);
					}
				}
			}
			else
			{
				Logger.LogInformation("\n==== OBB data file up to date so not writing. ====");
			}
		}

		public void WriteJavaDownloadSupportFiles(string ShimFileName, IEnumerable<TemplateFile> TemplateFiles, Dictionary<string, string> replacements)
		{
			// Deal with the Shim first as that is a known target and is easy to deal with
			// If it exists then read it
			string[]? DestFileContent = File.Exists(ShimFileName) ? File.ReadAllLines(ShimFileName) : null;

			StringBuilder ShimFileContent = new StringBuilder("package com.epicgames.unreal;\n\n");

			ShimFileContent.AppendFormat("import {0}.OBBDownloaderService;\n", replacements["$$PackageName$$"]);
			ShimFileContent.AppendFormat("import {0}.DownloaderActivity;\n", replacements["$$PackageName$$"]);

			// Do OBB file checking without using DownloadActivity to avoid transit to another activity
			ShimFileContent.Append("import android.app.Activity;\n");
			ShimFileContent.Append("import com.google.android.vending.expansion.downloader.Helpers;\n");
			ShimFileContent.AppendFormat("import {0}.OBBData;\n", replacements["$$PackageName$$"]);

			ShimFileContent.Append("\n\npublic class DownloadShim\n{\n");
			ShimFileContent.Append("\tpublic static OBBDownloaderService DownloaderService;\n");
			ShimFileContent.Append("\tpublic static DownloaderActivity DownloadActivity;\n");
			ShimFileContent.Append("\tpublic static Class<DownloaderActivity> GetDownloaderType() { return DownloaderActivity.class; }\n");

			// Do OBB file checking without using DownloadActivity to avoid transit to another activity
			ShimFileContent.Append("\tpublic static boolean expansionFilesDelivered(Activity activity, int version) {\n");
			ShimFileContent.Append("\t\tfor (OBBData.XAPKFile xf : OBBData.xAPKS) {\n");
			ShimFileContent.Append("\t\t\tString fileName = Helpers.getExpansionAPKFileName(activity, xf.mIsMain, Integer.toString(version), OBBData.AppType);\n");
			ShimFileContent.Append("\t\t\tGameActivity.Log.debug(\"Checking for file : \" + fileName);\n");
			ShimFileContent.Append("\t\t\tString fileForNewFile = Helpers.generateSaveFileName(activity, fileName);\n");
			ShimFileContent.Append("\t\t\tString fileForDevFile = Helpers.generateSaveFileNameDevelopment(activity, fileName);\n");
			ShimFileContent.Append("\t\t\tGameActivity.Log.debug(\"which is really being resolved to : \" + fileForNewFile + \"\\n Or : \" + fileForDevFile);\n");
			ShimFileContent.Append("\t\t\tif (Helpers.doesFileExist(activity, fileName, xf.mFileSize, false)) {\n");
			ShimFileContent.Append("\t\t\t\tGameActivity.Log.debug(\"Found OBB here: \" + fileForNewFile);\n");
			ShimFileContent.Append("\t\t\t}\n");
			ShimFileContent.Append("\t\t\telse if (Helpers.doesFileExistDev(activity, fileName, xf.mFileSize, false)) {\n");
			ShimFileContent.Append("\t\t\t\tGameActivity.Log.debug(\"Found OBB here: \" + fileForDevFile);\n");
			ShimFileContent.Append("\t\t\t}\n");
			ShimFileContent.Append("\t\t\telse return false;\n");
			ShimFileContent.Append("\t\t}\n");
			ShimFileContent.Append("\t\treturn true;\n");
			ShimFileContent.Append("\t}\n");

			ShimFileContent.Append("}\n");
			Logger.LogInformation("\n==== Writing to shim file {ShimFileName} ====", ShimFileName);

			// If they aren't the same then dump out the settings
			if (DestFileContent == null || !DestFileContent.SequenceEqual((ShimFileContent.ToString()).Split('\n')))
			{
				MakeDirectoryIfRequired(ShimFileName);
				using (StreamWriter outputFile = new StreamWriter(ShimFileName, false))
				{
					string[] shimSrc = ShimFileContent.ToString().Split('\n');
					foreach (string line in shimSrc)
					{
						outputFile.WriteLine(line);
					}
				}
			}
			else
			{
				Logger.LogInformation("\n==== Shim data file up to date so not writing. ====");
			}

			// Now we move on to the template files
			foreach (TemplateFile template in TemplateFiles)
			{
				string[] templateSrc = File.ReadAllLines(template.SourceFile);
				string[]? templateDest = File.Exists(template.DestinationFile) ? File.ReadAllLines(template.DestinationFile) : null;

				for (int i = 0; i < templateSrc.Length; ++i)
				{
					string srcLine = templateSrc[i];
					bool changed = false;
					foreach (KeyValuePair<string, string> kvp in replacements)
					{
						if (srcLine.Contains(kvp.Key))
						{
							srcLine = srcLine.Replace(kvp.Key, kvp.Value);
							changed = true;
						}
					}
					if (changed)
					{
						templateSrc[i] = srcLine;
					}
				}

				Logger.LogInformation("\n==== Writing to template target file {File} ====", template.DestinationFile);

				if (templateDest == null || templateSrc.Length != templateDest.Length || !templateSrc.SequenceEqual(templateDest))
				{
					MakeDirectoryIfRequired(template.DestinationFile);
					using (StreamWriter outputFile = new StreamWriter(template.DestinationFile, false))
					{
						foreach (string line in templateSrc)
						{
							outputFile.WriteLine(line);
						}
					}
				}
				else
				{
					Logger.LogInformation("\n==== Template target file up to date so not writing. ====");
				}
			}
		}

		public void WriteCrashlyticsResources(string UEBuildPath, string PackageName, string ApplicationDisplayName, bool bIsEmbedded, UnrealArch UnrealArch)
		{
			System.DateTime CurrentDateTime = System.DateTime.Now;
			string BuildID = Guid.NewGuid().ToString();

			string VersionDisplayName = GetVersionDisplayName(bIsEmbedded);

			StringBuilder CrashPropertiesContent = new StringBuilder("");
			CrashPropertiesContent.Append("# This file is automatically generated by Crashlytics to uniquely\n");
			CrashPropertiesContent.Append("# identify individual builds of your Android application.\n");
			CrashPropertiesContent.Append("#\n");
			CrashPropertiesContent.Append("# Do NOT modify, delete, or commit to source control!\n");
			CrashPropertiesContent.Append("#\n");
			CrashPropertiesContent.Append("# " + CurrentDateTime.ToString("D") + "\n");
			CrashPropertiesContent.Append("version_name=" + VersionDisplayName + "\n");
			CrashPropertiesContent.Append("package_name=" + PackageName + "\n");
			CrashPropertiesContent.Append("build_id=" + BuildID + "\n");
			CrashPropertiesContent.Append("version_code=" + GetStoreVersion(UnrealArch).ToString() + "\n");

			string CrashPropertiesFileName = Path.Combine(UEBuildPath, "assets", "crashlytics-build.properties");
			MakeDirectoryIfRequired(CrashPropertiesFileName);
			File.WriteAllText(CrashPropertiesFileName, CrashPropertiesContent.ToString());
			Logger.LogInformation("==== Write {CrashPropertiesFileName}  ====", CrashPropertiesFileName);

			StringBuilder BuildIDContent = new StringBuilder("");
			BuildIDContent.Append("<?xml version=\"1.0\" encoding=\"utf-8\" standalone=\"no\"?>\n");
			BuildIDContent.Append("<resources xmlns:tools=\"http://schemas.android.com/tools\">\n");
			BuildIDContent.Append("<!--\n");
			BuildIDContent.Append("  This file is automatically generated by Crashlytics to uniquely\n");
			BuildIDContent.Append("  identify individual builds of your Android application.\n");
			BuildIDContent.Append("\n");
			BuildIDContent.Append("  Do NOT modify, delete, or commit to source control!\n");
			BuildIDContent.Append("-->\n");
			BuildIDContent.Append("<string tools:ignore=\"UnusedResources, TypographyDashes\" name=\"com.crashlytics.android.build_id\" translatable=\"false\">" + BuildID + "</string>\n");
			BuildIDContent.Append("</resources>\n");

			string BuildIDFileName = Path.Combine(UEBuildPath, "res", "values", "com_crashlytics_build_id.xml");
			MakeDirectoryIfRequired(BuildIDFileName);
			File.WriteAllText(BuildIDFileName, BuildIDContent.ToString());
			Logger.LogInformation("==== Write {BuildIDFileName}  ====", BuildIDFileName);
		}

		private static string GetNDKArch(UnrealArch UnrealArch)
		{
			if (UnrealArch == UnrealArch.Arm64)
			{
				return "arm64-v8a";
			}
			else if (UnrealArch == UnrealArch.X64)
			{
				return "x86_64";
			}

			throw new BuildException("Unknown Unreal architecture '{0}'", UnrealArch);
		}

		public static UnrealArch GetUnrealArch(string NDKArch)
		{
			switch (NDKArch)
			{
				case "arm64-v8a":
				case "arm64": return UnrealArch.Arm64;
				case "x86_64":
				case "x64": return UnrealArch.X64;

				//				default: throw new BuildException("Unknown NDK architecture '{0}'", NDKArch);
				// future-proof by returning arm64 for unknown
				default: return UnrealArch.Arm64;
			}
		}

		private static void StripDebugSymbols(string SourceFileName, string TargetFileName, UnrealArch UnrealArch, ILogger Logger, bool bStripAll = false)
		{
			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = AndroidToolChain.GetStripExecutablePath(UnrealArch).Trim('"');
			string StripCommand = bStripAll ? "--strip-unneeded" : "--strip-debug"; 
			StartInfo.Arguments = $"{StripCommand} -o \"{TargetFileName}\" \"{SourceFileName}\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo, Logger);
		}

		private static void CopySO(string FinalName, string SourceName)
		{
			// check to see if file is newer than last time we copied
			bool bFileExists = File.Exists(FinalName);
			TimeSpan Diff = File.GetLastWriteTimeUtc(FinalName) - File.GetLastWriteTimeUtc(SourceName);
			if (!bFileExists || Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
			{
				SafeDeleteFile(FinalName);

				string? DirectoryName = Path.GetDirectoryName(FinalName);
				if (DirectoryName != null)
				{
					Directory.CreateDirectory(DirectoryName);
					File.Copy(SourceName, FinalName, true);

					// make sure it's writable if the source was readonly (e.g. autosdks)
					new FileInfo(FinalName).IsReadOnly = false;
					File.SetLastWriteTimeUtc(FinalName, File.GetLastWriteTimeUtc(SourceName));
				}
			}
		}

		private static string GetPlatformNDKHostName()
		{
			if (RuntimePlatform.IsLinux)
			{
				return "linux-x86_64";
			}
			else if (RuntimePlatform.IsMac)
			{
				return "darwin-x86_64";
			}

			return "windows-x86_64";
		}

		private static void CopySTL(AndroidToolChain ToolChain, string UnrealBuildPath, UnrealArch UnrealArch, string NDKArch, bool bForDistribution)
		{
			// copy it in!
			string SourceSTLSOName = Environment.ExpandEnvironmentVariables("%NDKROOT%/sources/cxx-stl/llvm-libc++/libs/") + NDKArch + "/libc++_shared.so";
			if (!File.Exists(SourceSTLSOName))
			{
				// NDK25 has changed a directory where it stores libs, check it instead
				string NDKTargetTripletName = (NDKArch == "x86_64") ? "x86_64-linux-android" : "aarch64-linux-android";
				SourceSTLSOName = Environment.ExpandEnvironmentVariables("%NDKROOT%/toolchains/llvm/prebuilt/") + GetPlatformNDKHostName() + "/sysroot/usr/lib/" + NDKTargetTripletName + "/libc++_shared.so";
			}
			string FinalSTLSOName = UnrealBuildPath + "/jni/" + NDKArch + "/libc++_shared.so";

			CopySO(FinalSTLSOName, SourceSTLSOName);
		}

		private void CopyPSOService(string UnrealBuildPath, string PreBuiltPath, UnrealArch UnrealArch, string NDKArch)
		{
			// copy it in!
			string SourceSOName = Path.Combine(PreBuiltPath, "PSOService/Android/Release/" + NDKArch) + "/libpsoservice.so";
			string FinalSOName = UnrealBuildPath + "/jni/" + NDKArch + "/libpsoservice.so";

			CopySO(FinalSOName, SourceSOName);
		}

		private void CopyGfxDebugger(string UnrealBuildPath, UnrealArch UnrealArch, string NDKArch)
		{
			string AndroidGraphicsDebugger;
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "AndroidGraphicsDebugger", out AndroidGraphicsDebugger);

			switch (AndroidGraphicsDebugger.ToLower())
			{
				case "mali":
					{
						string MaliGraphicsDebuggerPath;
						AndroidPlatformSDK.GetPath(Ini, "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MaliGraphicsDebuggerPath", out MaliGraphicsDebuggerPath);
						if (Directory.Exists(MaliGraphicsDebuggerPath))
						{
							Directory.CreateDirectory(Path.Combine(UnrealBuildPath, "libs", NDKArch));
							string MaliLibSrcPath = Path.Combine(MaliGraphicsDebuggerPath, "target", "android-non-root", "arm", NDKArch, "libMGD.so");
							if (!File.Exists(MaliLibSrcPath))
							{
								// in v4.3.0 library location was changed
								MaliLibSrcPath = Path.Combine(MaliGraphicsDebuggerPath, "target", "android", "arm", "unrooted", NDKArch, "libMGD.so");
							}
							string MaliLibDstPath = Path.Combine(UnrealBuildPath, "libs", NDKArch, "libMGD.so");

							Logger.LogInformation("Copying {MaliLibSrcPath} to {MaliLibDstPath}", MaliLibSrcPath, MaliLibDstPath);
							File.Copy(MaliLibSrcPath, MaliLibDstPath, true);
							File.SetLastWriteTimeUtc(MaliLibDstPath, File.GetLastWriteTimeUtc(MaliLibSrcPath));

							string MaliVkLayerLibSrcPath = Path.Combine(MaliGraphicsDebuggerPath, "target", "android", "arm", "rooted", NDKArch, "libGLES_aga.so");
							if (File.Exists(MaliVkLayerLibSrcPath))
							{
								string MaliVkLayerLibDstPath = Path.Combine(UnrealBuildPath, "libs", NDKArch, "libVkLayerAGA.so");
								Logger.LogInformation("Copying {MaliVkLayerLibSrcPath} to {MaliVkLayerLibDstPath}", MaliVkLayerLibSrcPath, MaliVkLayerLibDstPath);
								File.Copy(MaliVkLayerLibSrcPath, MaliVkLayerLibDstPath, true);
								File.SetLastWriteTimeUtc(MaliVkLayerLibDstPath, File.GetLastWriteTimeUtc(MaliVkLayerLibSrcPath));
							}
						}
					}
					break;

				// @TODO: Add NVIDIA Gfx Debugger
				/*
				case "nvidia":
					{
						Directory.CreateDirectory(UnrealBuildPath + "/libs/" + NDKArch);
						File.Copy("F:/NVPACK/android-kk-egl-t124-a32/Stripped_libNvPmApi.Core.so", UnrealBuildPath + "/libs/" + NDKArch + "/libNvPmApi.Core.so", true);
						File.Copy("F:/NVPACK/android-kk-egl-t124-a32/Stripped_libNvidia_gfx_debugger.so", UnrealBuildPath + "/libs/" + NDKArch + "/libNvidia_gfx_debugger.so", true);
					}
					break;
				*/
				default:
					break;
			}
		}

		void LogBuildSetup()
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			bool bBuildForES31 = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForES31", out bBuildForES31);
			bool bSupportsVulkan = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportsVulkan", out bSupportsVulkan);

			Logger.LogInformation("bBuildForES31: {bBuildForES31}", (bBuildForES31 ? "true" : "false"));
			Logger.LogInformation("bSupportsVulkan: {bSupportsVulkan}", (bSupportsVulkan ? "true" : "false"));
		}

		void CopyVulkanValidationLayers(string UnrealBuildPath, UnrealArch UnrealArch, string NDKArch, string Configuration)
		{
			bool bSupportsVulkan = false;
			bool bSupportsVulkanSM5 = false;
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportsVulkan", out bSupportsVulkan);
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportsVulkanSM5", out bSupportsVulkanSM5);

			bool bCopyVulkanLayers = (bSupportsVulkan || bSupportsVulkanSM5) && (Configuration != "Shipping");
			if (bCopyVulkanLayers)
			{
				string VulkanLayersDir = Path.Combine(Unreal.EngineDirectory.ToString(), "Binaries", "ThirdParty", "Vulkan", "Android", NDKArch);
				if (Directory.Exists(VulkanLayersDir))
				{
					Logger.LogInformation("Copying {ANDROID_VULKAN_VALIDATION_LAYER} vulkan layer from {VulkanLayersDir}", ANDROID_VULKAN_VALIDATION_LAYER, VulkanLayersDir);
					string DestDir = Path.Combine(UnrealBuildPath, "libs", NDKArch);
					Directory.CreateDirectory(DestDir);
					string SourceFilename = Path.Combine(VulkanLayersDir, ANDROID_VULKAN_VALIDATION_LAYER);
					string DestFilename = Path.Combine(DestDir, ANDROID_VULKAN_VALIDATION_LAYER);
					SafeDeleteFile(DestFilename);
					File.Copy(SourceFilename, DestFilename);
					FileInfo DestFileInfo = new FileInfo(DestFilename);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
					File.SetLastWriteTimeUtc(DestFilename, File.GetLastWriteTimeUtc(SourceFilename));
				}
				else
				{
					Logger.LogWarning("{ANDROID_VULKAN_VALIDATION_LAYER} vulkan layer not found at {VulkanLayersDir}, skipping", ANDROID_VULKAN_VALIDATION_LAYER, VulkanLayersDir);
				}

				String DebugVulkanLayerDirectory = "";
				AndroidPlatformSDK.GetPath(Ini, "/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "DebugVulkanLayerDirectory", out DebugVulkanLayerDirectory);
				Logger.LogInformation("DebugVulkanLayerDirectory {LayerDir}", DebugVulkanLayerDirectory);

				if (!String.IsNullOrEmpty(DebugVulkanLayerDirectory))
				{
					DebugVulkanLayerDirectory = Environment.ExpandEnvironmentVariables(DebugVulkanLayerDirectory);
					if (!Path.IsPathRooted(DebugVulkanLayerDirectory))
						DebugVulkanLayerDirectory = Path.Combine(Unreal.RootDirectory.ToString(), DebugVulkanLayerDirectory); 
					string LayersDir = Path.Combine(DebugVulkanLayerDirectory, NDKArch);

					if (Directory.Exists(LayersDir))
					{
						string DestDir = Path.Combine(UnrealBuildPath, "libs", NDKArch);
						Logger.LogInformation("Copying Debug vulkan layers from {DebugVulkanLayerDirectory} to {DestDir}", DebugVulkanLayerDirectory, DestDir);

						if (!Directory.Exists(DestDir))
						{
							Directory.CreateDirectory(DestDir);
						}
						CopyFileDirectory(LayersDir, DestDir);
					}
					else
					{
						Logger.LogWarning("DebugVulkanLayerDirectory vulkan layers not found at {DebugVulkanLayerDirectory}, skipping", DebugVulkanLayerDirectory);
					}
				}
			}
		}

		void CopyClangSanitizerLib(AndroidToolChain ToolChain, string UnrealBuildPath, UnrealArch UnrealArch, string NDKArch, AndroidToolChain.ClangSanitizer Sanitizer)
		{
			string Architecture = "-aarch64";
			switch (NDKArch)
			{
				case "armeabi-v7a":
					Architecture = "-arm";
					break;
				case "x86_64":
					Architecture = "-x86_64";
					break;
				case "x86":
					Architecture = "-i686";
					break;
			}

			string LibName = "asan";
			switch (Sanitizer)
			{
				case AndroidToolChain.ClangSanitizer.HwAddress:
				{
					// no need to bundle asan .so in NDK r26b+
					LibName = ToolChain.HasEmbeddedHWASanSupport() ? string.Empty : "hwasan";
					break;
				}
				case AndroidToolChain.ClangSanitizer.UndefinedBehavior:
					LibName = "ubsan_standalone";
					break;
				case AndroidToolChain.ClangSanitizer.UndefinedBehaviorMinimal:
					LibName = "ubsan_minimal";
					break;
				case AndroidToolChain.ClangSanitizer.Thread:
					LibName = "tsan";
					break;
			}

			string SanitizerFullLibName = string.IsNullOrEmpty(LibName) ? string.Empty : "libclang_rt." + LibName + Architecture + "-android.so";

			// NDK r26b+ needs different wrap.sh script
			string WrapShName = Sanitizer == AndroidToolChain.ClangSanitizer.HwAddress && ToolChain.HasEmbeddedHWASanSupport() ? "hwasan.sh" : "asan.sh";

			string WrapShFilePath = Path.Combine(Environment.ExpandEnvironmentVariables("%NDKROOT%"), "wrap.sh", WrapShName);

			string PlatformHostName = GetPlatformNDKHostName();

			string VersionFileName = Path.Combine(Environment.ExpandEnvironmentVariables("%NDKROOT%"), "toolchains", "llvm", "prebuilt", PlatformHostName, "AndroidVersion.txt");
			System.IO.StreamReader VersionFile = new System.IO.StreamReader(VersionFileName);
			string LibsVersion = VersionFile.ReadLine()!;
			VersionFile.Close();

			string SanitizerLib = string.IsNullOrEmpty(SanitizerFullLibName) ?
				string.Empty :
				Path.Combine(Environment.ExpandEnvironmentVariables("%NDKROOT%"), "toolchains", "llvm", "prebuilt", PlatformHostName, (ToolChain.HasEmbeddedHWASanSupport() ? "lib" : "lib64"), "clang", LibsVersion, "lib", "linux", SanitizerFullLibName);

			if (!string.IsNullOrEmpty(SanitizerLib))
			{
				if (File.Exists(SanitizerLib))
				{
					string LibDestDir = Path.Combine(UnrealBuildPath, "libs", NDKArch);
					Directory.CreateDirectory(LibDestDir);

					string LibDestFilePath = Path.Combine(LibDestDir, SanitizerFullLibName);
					Logger.LogInformation("Copying asan lib from {SanitizerLib} to {LibDestFilePath}", SanitizerLib, LibDestFilePath);
					File.Copy(SanitizerLib, LibDestFilePath, true);
				}
				else
				{
					throw new BuildException("No asan lib found in {0}", SanitizerLib);
				}
			}

			if (File.Exists(WrapShFilePath))
			{
				string WrapDestDir = Path.Combine(UnrealBuildPath, "resources", "lib", NDKArch);
				Directory.CreateDirectory(WrapDestDir);

				string WrapDestFilePath = Path.Combine(WrapDestDir, "wrap.sh");
				Logger.LogInformation("Copying wrap.sh from {WrapShFilePath} to {WrapDestFilePath}", WrapShFilePath, WrapDestFilePath);
				File.Copy(WrapShFilePath, WrapDestFilePath, true);

				FileAttributes Attributes = File.GetAttributes(WrapDestFilePath);
				Attributes &= ~FileAttributes.ReadOnly;
				File.SetAttributes(WrapDestFilePath, Attributes);
			}
			else
			{
				throw new BuildException("No asan wrap.sh found in {0}", WrapShFilePath);
			}
		}

		void CopyScudoMemoryTracerLib(string UnrealBuildPath, UnrealArch UnrealArch, string NDKArch, string PackageName)
		{
			if (UnrealArch != UnrealArch.Arm64 && UnrealArch != UnrealArch.X64)
			{
				throw new BuildException("ScudoMemoryTrace not supported for arch {0}", UnrealArch.ToString());
			}

			string ScudoMemoryTraceLib = Path.Combine(Unreal.EngineDirectory.ToString(), "Build", "Android", "Prebuilt", "ScudoMemoryTrace", NDKArch, "libScudoMemoryTrace.so");

			string WrapSh = Path.Combine(Unreal.EngineDirectory.ToString(), "Build", "Android", "Prebuilt", "ScudoMemoryTrace", "wrap.sh");

			if (File.Exists(ScudoMemoryTraceLib) && File.Exists(WrapSh))
			{
				string LibDestDir = Path.Combine(UnrealBuildPath, "libs", NDKArch);
				string LibDest = Path.Combine(LibDestDir, "libScudoMemoryTrace.so");
				Directory.CreateDirectory(LibDestDir);
				if (!CopyIfDifferent(ScudoMemoryTraceLib, LibDest, true, false))
				{
					Logger.LogInformation("{LibDest} is up to date", LibDest);
				}

				string WrapDestDir = Path.Combine(UnrealBuildPath, "resources", "lib", NDKArch);
				Directory.CreateDirectory(WrapDestDir);
				string WrapDestFilePath = Path.Combine(WrapDestDir, "wrap.sh");

				Dictionary<string, string> Replacements = new Dictionary<string, string>
				{
					{ "${PACKAGE_NAME}", PackageName },
				};
				string WrapShText = File.ReadAllText(WrapSh);
				foreach (KeyValuePair<string, string> KVP in Replacements)
				{
					WrapShText = WrapShText.Replace(KVP.Key, KVP.Value);
				}

				if (!File.Exists(WrapDestFilePath) || File.ReadAllText(WrapDestFilePath) != WrapShText)
				{
					Logger.LogInformation("Writing {WrapDestFilePath}", WrapDestFilePath);
					File.WriteAllText(WrapDestFilePath, WrapShText);
				}
				else
				{
					Logger.LogInformation("{WrapDestFilePath} is up to date", WrapDestFilePath);
				}
			}
			else
			{
				throw new BuildException("No ScudoMemoryTrace found in {0} or wrap.sh in {1}", ScudoMemoryTraceLib, WrapSh);
			}
		}

		private static int RunCommandLineProgramAndReturnResult(string WorkingDirectory, string Command, string Params, ILogger Logger, string? OverrideDesc = null, bool bUseShellExecute = false)
		{
			// Process Arguments follow windows conventions in .NET Core
			// Which means single quotes ' are not considered quotes.
			// see https://github.com/dotnet/runtime/issues/29857
			// also see UE-102580
			// for rules see https://docs.microsoft.com/en-us/cpp/cpp/main-function-command-line-args
			if (Params.Contains('\''))
			{
				Params = Params.Replace("\"", "\\\"");
				Params = Params.Replace('\'', '\"');
			}

			if (OverrideDesc == null)
			{
				Logger.LogInformation("\nRunning: {Command} {Params}", Command, Params);
			}
			else if (!String.IsNullOrEmpty(OverrideDesc))
			{
				Logger.LogInformation("{Message}", OverrideDesc);
				Logger.LogDebug("\nRunning: {Command} {Params}", Command, Params);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDirectory;
			StartInfo.FileName = Command;
			StartInfo.Arguments = Params;
			StartInfo.UseShellExecute = bUseShellExecute;
			StartInfo.WindowStyle = ProcessWindowStyle.Minimized;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			Proc.Start();
			Proc.WaitForExit();

			return Proc.ExitCode;
		}

		private static void RunCommandLineProgramWithException(string WorkingDirectory, string Command, string Params, ILogger Logger, string? OverrideDesc = null, bool bUseShellExecute = false)
		{
			// Process Arguments follow windows conventions in .NET Core
			// Which means single quotes ' are not considered quotes.
			// see https://github.com/dotnet/runtime/issues/29857
			// also see UE-102580
			// for rules see https://docs.microsoft.com/en-us/cpp/cpp/main-function-command-line-args
			if (Params.Contains('\''))
			{
				Params = Params.Replace("\"", "\\\"");
				Params = Params.Replace('\'', '\"');
			}

			if (OverrideDesc == null)
			{
				Logger.LogInformation("\nRunning: {Command} {Params}", Command, Params);
			}
			else if (!String.IsNullOrEmpty(OverrideDesc))
			{
				Logger.LogInformation("{Message}", OverrideDesc);
				Logger.LogDebug("\nRunning: {Command} {Params}", Command, Params);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDirectory;
			StartInfo.FileName = Command;
			StartInfo.Arguments = Params;
			StartInfo.UseShellExecute = bUseShellExecute;
			StartInfo.WindowStyle = ProcessWindowStyle.Minimized;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			Proc.Start();
			Proc.WaitForExit();

			// android bat failure
			if (Proc.ExitCode != 0)
			{
				throw new BuildException("{0} failed with args {1}", Command, Params);
			}
		}

		private enum FilterAction
		{
			Skip,
			Replace,
			Error
		}

		private class FilterOperation
		{
			public FilterAction Action;
			public string Condition;
			public string Match;
			public string ReplaceWith;

			public FilterOperation(FilterAction InAction, string InCondition, string InMatch, string InReplaceWith)
			{
				Action = InAction;
				Condition = InCondition;
				Match = InMatch;
				ReplaceWith = InReplaceWith;
			}
		}

		private static List<FilterOperation>? ActiveStdOutFilter = null;

		static List<string> ParseCSVString(string Input)
		{
			List<string> Results = new List<string>();
			StringBuilder WorkString = new StringBuilder();

			int FinalIndex = Input.Length;
			int CurrentIndex = 0;
			bool InQuote = false;

			while (CurrentIndex < FinalIndex)
			{
				char CurChar = Input[CurrentIndex++];

				if (InQuote)
				{
					if (CurChar == '\\')
					{
						if (CurrentIndex < FinalIndex)
						{
							CurChar = Input[CurrentIndex++];
							WorkString.Append(CurChar);
						}
					}
					else if (CurChar == '"')
					{
						InQuote = false;
					}
					else
					{
						WorkString.Append(CurChar);
					}
				}
				else
				{
					if (CurChar == '"')
					{
						InQuote = true;
					}
					else if (CurChar == ',')
					{
						Results.Add(WorkString.ToString());
						WorkString.Clear();
					}
					else if (!Char.IsWhiteSpace(CurChar))
					{
						WorkString.Append(CurChar);
					}
				}
			}
			if (CurrentIndex > 0)
			{
				Results.Add(WorkString.ToString());
			}

			return Results;
		}

		static void ParseFilterFile(string Filename)
		{

			if (File.Exists(Filename))
			{
				ActiveStdOutFilter = new List<FilterOperation>();

				string[] FilterContents = File.ReadAllLines(Filename);
				foreach (string FileLine in FilterContents)
				{
					List<string> Parts = ParseCSVString(FileLine);

					if (Parts.Count > 1)
					{
						if (Parts[0].Equals("S"))
						{
							ActiveStdOutFilter.Add(new FilterOperation(FilterAction.Skip, Parts[1], "", ""));
						}
						else if (Parts[0].Equals("R"))
						{
							if (Parts.Count == 4)
							{
								ActiveStdOutFilter.Add(new FilterOperation(FilterAction.Replace, Parts[1], Parts[2], Parts[3]));
							}
						}
						else if (Parts[0].Equals("E"))
						{
							if (Parts.Count == 4)
							{
								ActiveStdOutFilter.Add(new FilterOperation(FilterAction.Error, Parts[1], Parts[2], Parts[3]));
							}
						}
					}
				}

				if (ActiveStdOutFilter.Count == 0)
				{
					ActiveStdOutFilter = null;
				}
			}
		}

		static void FilterStdOutErr(object sender, DataReceivedEventArgs e, ILogger Logger)
		{
			if (e.Data != null)
			{
				if (ActiveStdOutFilter != null)
				{
					foreach (FilterOperation FilterOp in ActiveStdOutFilter)
					{
						if (e.Data.Contains(FilterOp.Condition))
						{
							switch (FilterOp.Action)
							{
								case FilterAction.Skip:
									break;

								case FilterAction.Replace:
									Logger.LogInformation("{Output}", e.Data.Replace(FilterOp.Match, FilterOp.ReplaceWith));
									break;

								case FilterAction.Error:
									Logger.LogError("{Output}", e.Data.Replace(FilterOp.Match, FilterOp.ReplaceWith));
									break;

								default:
									break;
							}
							return;
						}
					}
				}
				Logger.LogInformation("{Output}", e.Data);
			}
		}

		private static void RunCommandLineProgramWithExceptionAndFiltering(string WorkingDirectory, string Command, string Params, ILogger Logger, string? OverrideDesc = null, bool bUseShellExecute = false)
		{
			// Process Arguments follow windows conventions in .NET Core
			// Which means single quotes ' are not considered quotes.
			// see https://github.com/dotnet/runtime/issues/29857
			// also see UE-102580
			// for rules see https://docs.microsoft.com/en-us/cpp/cpp/main-function-command-line-args
			if (Params.Contains('\''))
			{
				Params = Params.Replace("\"", "\\\"");
				Params = Params.Replace('\'', '\"');
			}

			if (OverrideDesc == null)
			{
				Logger.LogInformation("\nRunning: {Command} {Params}", Command, Params);
			}
			else if (!String.IsNullOrEmpty(OverrideDesc))
			{
				Logger.LogInformation("{Message}", OverrideDesc);
				Logger.LogDebug("\nRunning: {Command} {Params}", Command, Params);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.WorkingDirectory = WorkingDirectory;
			StartInfo.FileName = Command;
			StartInfo.Arguments = Params;
			StartInfo.UseShellExecute = bUseShellExecute;
			StartInfo.WindowStyle = ProcessWindowStyle.Minimized;
			StartInfo.RedirectStandardInput = true;
			StartInfo.RedirectStandardOutput = true;
			StartInfo.RedirectStandardError = true;

			Process Proc = new Process();
			Proc.StartInfo = StartInfo;
			Proc.OutputDataReceived += (s, e) => FilterStdOutErr(s, e, Logger);
			Proc.ErrorDataReceived += (s, e) => FilterStdOutErr(s, e, Logger);

			Proc.Start();
			Proc.BeginOutputReadLine();
			Proc.BeginErrorReadLine();

			StreamWriter StreamIn = Proc.StandardInput;
			StreamIn.WriteLine("yes");
			StreamIn.Close();

			Proc.WaitForExit();

			// android bat failure
			if (Proc.ExitCode != 0)
			{
				throw new BuildException("{0} failed with args {1}", Command, Params);
			}
		}

		private bool CheckApplicationName(string UnrealBuildPath, string ProjectName, out string? ApplicationDisplayName)
		{
			string StringsXMLPath = Path.Combine(UnrealBuildPath, "res/values/strings.xml");

			ApplicationDisplayName = null;
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ApplicationDisplayName", out ApplicationDisplayName);

			// use project name if display name is left blank
			if (String.IsNullOrWhiteSpace(ApplicationDisplayName))
			{
				ApplicationDisplayName = ProjectName;
			}

			// replace escaped characters (note: changes &# pattern before &, then patches back to allow escaped character codes in the string)
			ApplicationDisplayName = ApplicationDisplayName.Replace("&#", "$@#$").Replace("&", "&amp;").Replace("'", "\\'").Replace("\"", "\\\"").Replace("<", "&lt;").Replace(">", "&gt;").Replace("$@#$", "&#");

			// if it doesn't exist, need to repackage
			if (!File.Exists(StringsXMLPath))
			{
				return true;
			}

			// read it and see if needs to be updated
			string Contents = File.ReadAllText(StringsXMLPath);

			// find the key
			string AppNameTag = "<string name=\"app_name\">";
			int KeyIndex = Contents.IndexOf(AppNameTag);

			// if doesn't exist, need to repackage
			if (KeyIndex < 0)
			{
				return true;
			}

			// get the current value
			KeyIndex += AppNameTag.Length;
			int TagEnd = Contents.IndexOf("</string>", KeyIndex);
			if (TagEnd < 0)
			{
				return true;
			}
			string CurrentApplicationName = Contents.Substring(KeyIndex, TagEnd - KeyIndex);

			// no need to do anything if matches
			if (CurrentApplicationName == ApplicationDisplayName)
			{
				// name matches, no need to force a repackage
				return false;
			}

			// need to repackage
			return true;
		}

		private string GetAllBuildSettings(AndroidToolChain ToolChain, UnrealPluginLanguage UPL, bool bForDistribution, bool bMakeSeparateApks, bool bPackageDataInsideApk,
			bool bDisableVerifyOBBOnStartUp, bool bUseExternalFilesDir, bool bDontBundleLibrariesInAPK, string TemplatesHashCode)
		{
			// make the settings string - this will be char by char compared against last time
			StringBuilder CurrentSettings = new StringBuilder();
			CurrentSettings.AppendLine(String.Format("NDKROOT={0}", Environment.GetEnvironmentVariable("NDKROOT")));
			CurrentSettings.AppendLine(String.Format("ANDROID_HOME={0}", Environment.GetEnvironmentVariable("ANDROID_HOME")));
			CurrentSettings.AppendLine(String.Format("JAVA_HOME={0}", Environment.GetEnvironmentVariable("JAVA_HOME")));
			CurrentSettings.AppendLine(String.Format("NDKVersion={0}", ToolChain.GetNdkApiLevel()));
			CurrentSettings.AppendLine(String.Format("SDKVersion={0}", GetSdkApiLevel(ToolChain)));
			CurrentSettings.AppendLine(String.Format("bForDistribution={0}", bForDistribution));
			CurrentSettings.AppendLine(String.Format("bMakeSeparateApks={0}", bMakeSeparateApks));
			CurrentSettings.AppendLine(String.Format("bPackageDataInsideApk={0}", bPackageDataInsideApk));
			CurrentSettings.AppendLine(String.Format("bDisableVerifyOBBOnStartUp={0}", bDisableVerifyOBBOnStartUp));
			CurrentSettings.AppendLine(String.Format("bUseExternalFilesDir={0}", bUseExternalFilesDir));
			CurrentSettings.AppendLine(String.Format("UPLHashCode={0}", UPLHashCode));
			CurrentSettings.AppendLine(String.Format("TemplatesHashCode={0}", TemplatesHashCode));
			CurrentSettings.AppendLine(String.Format("bDontBundleLibrariesInAPK={0}", bDontBundleLibrariesInAPK));

			// all AndroidRuntimeSettings ini settings in here
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			ConfigHierarchySection Section = Ini.FindSection("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings");
			if (Section != null)
			{
				foreach (string Key in Section.KeyNames)
				{
					// filter out NDK and SDK override since actual resolved versions already written above
					if (Key.Equals("SDKAPILevelOverride") || Key.Equals("NDKAPILevelOverride"))
					{
						continue;
					}

					if (Section.TryGetValues(Key, out IReadOnlyList<string>? Values))
					{
						foreach (string Value in Values)
						{
							CurrentSettings.AppendLine(String.Format("{0}={1}", Key, Value));
						}
					}
				}
			}

			Section = Ini.FindSection("/Script/AndroidPlatformEditor.AndroidSDKSettings");
			if (Section != null)
			{
				foreach (string Key in Section.KeyNames)
				{
					// filter out NDK and SDK levels since actual resolved versions already written above
					if (Key.Equals("SDKAPILevel") || Key.Equals("NDKAPILevel"))
					{
						continue;
					}

					if (Section.TryGetValues(Key, out IReadOnlyList<string>? Values))
					{
						foreach (string Value in Values)
						{
							CurrentSettings.AppendLine(String.Format("{0}={1}", Key, Value));
						}
					}
				}
			}

			foreach (UnrealArch Arch in Architectures!.Architectures)
			{
				CurrentSettings.AppendFormat("Arch={0}{1}", Arch, Environment.NewLine);
			}

			// Modifying some settings in the GameMapsSettings could trigger the OBB regeneration
			// and make the cached OBBData.java mismatch to the actually data. 
			// So we insert the relevant keys into CurrentSettings to capture the change, to
			// enforce the refreshing of Android java codes
			Section = Ini.FindSection("/Script/EngineSettings.GameMapsSettings");
			if (Section != null)
			{
				foreach (string Key in Section.KeyNames)
				{
					if (!Key.Equals("GameDefaultMap") &&
						!Key.Equals("GlobalDefaultGameMode"))
					{
						continue;
					}

					if (Section.TryGetValues(Key, out IReadOnlyList<string>? Values))
					{
						foreach (string Value in Values)
						{
							CurrentSettings.AppendLine(String.Format("{0}={1}", Key, Value));
						}
					}
				}
			}

			// get a list of the ini settings in UPL files that may affect the build
			// architecture doesn't matter here since this node does not use init logic
			string UPLBuildSettings = UPL.ProcessPluginNode(GetNDKArch(Architectures.Architectures[0]), "registerBuildSettings", "");
			foreach (string Line in UPLBuildSettings.Split('\n', StringSplitOptions.RemoveEmptyEntries))
			{
				string SectionName = Line.Trim();

				// needed keys are provided in [ ] separated by commas
				string[]? NeededKeys = null;
				int KeyIndex = SectionName.IndexOf('[');
				if (KeyIndex > 0)
				{
					string KeyList = SectionName.Substring(KeyIndex + 1);
					SectionName = SectionName.Substring(0, KeyIndex);
					int CloseIndex = KeyList.IndexOf("]");
					if (CloseIndex > 1)
					{
						NeededKeys = KeyList.Substring(0, CloseIndex).Split(',', StringSplitOptions.RemoveEmptyEntries);
						if (NeededKeys.Length == 0)
						{
							NeededKeys = null;
						}
					}
				}

				// write the values for the requested keys (or all if none specified)
				Section = Ini.FindSection(SectionName);
				if (Section != null)
				{
					foreach (string Key in Section.KeyNames)
					{
						if (NeededKeys != null && !NeededKeys.Contains(Key))
						{
							continue;
						}
						if (Section.TryGetValues(Key, out IReadOnlyList<string>? Values))
						{
							foreach (string Value in Values)
							{
								CurrentSettings.AppendLine(String.Format("{0}:{1}={2}", SectionName, Key, Value));
							}
						}
					}
				}
			}

			return CurrentSettings.ToString();
		}

		private bool CheckDependencies(UnrealArchitectures Architectures, string ProjectName, string ProjectDirectory, string IntermediateAndroidPath,
			string UnrealBuildFilesPath, string GameBuildFilesPath, string EngineDirectory, List<string> SettingsFiles, string CookFlavor,
			string OutputPath, bool bMakeSeparateApks, bool bPackageDataInsideApk, bool bDontBundleLibrariesInAPK)
		{
			// check all input files (.so, java files, .ini files, etc)
			bool bAllInputsCurrent = true;
			foreach (UnrealArch Arch in Architectures.Architectures)
			{
				string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch);
				// if the source binary was UnrealGame, replace it with the new project name, when re-packaging a binary only build
				string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UnrealGame", ProjectName);
				string DestApkName = Path.Combine(ProjectDirectory, "Binaries/Android/") + ApkFilename + ".apk";

				// if we making multiple Apks, we need to put the architecture into the name
				if (bMakeSeparateApks)
				{
					DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch);
				}

				List<String> InputFiles = new List<string>();
				// check to see if the libUnreal.so is out of date before trying the slow make apk process
				if (!bDontBundleLibrariesInAPK)
				{
					InputFiles.Add(SourceSOName);
				}

				// add all files in jni and libs subfolders
				foreach (var SoDirName in new [] {"jni", "libs"})
				{
					string SoDirPath = Path.Combine(IntermediateAndroidPath, ArchRemapping[GetNDKArch(Arch)], SoDirName, GetNDKArch(Arch));
					if (Directory.Exists(SoDirPath))
					{
						var files = Directory.EnumerateFiles(SoDirPath, "*.*", SearchOption.AllDirectories);
						InputFiles.AddRange(files);
					}
				}

				// add all Engine and Project build files to be safe
				InputFiles.AddRange(Directory.EnumerateFiles(UnrealBuildFilesPath, "*.*", SearchOption.AllDirectories));
				if (Directory.Exists(GameBuildFilesPath))
				{
					InputFiles.AddRange(Directory.EnumerateFiles(GameBuildFilesPath, "*.*", SearchOption.AllDirectories));
				}

				// make sure changed java files will rebuild apk
				InputFiles.AddRange(SettingsFiles);

				// rebuild if .pak files exist for OBB in APK case
				if (bPackageDataInsideApk)
				{
					string PAKFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + "/" + ProjectName + "/Content/Paks";
					if (Directory.Exists(PAKFileLocation))
					{
						IEnumerable<string> PakFiles = Directory.EnumerateFiles(PAKFileLocation, "*.pak", SearchOption.TopDirectoryOnly);
						foreach (string Name in PakFiles)
						{
							InputFiles.Add(Name);
						}
					}
				}

				// look for any newer input file
				DateTime ApkTime = File.GetLastWriteTimeUtc(DestApkName);
				foreach (string InputFileName in InputFiles)
				{
					if (File.Exists(InputFileName))
					{
						// skip .log files
						if (Path.GetExtension(InputFileName) == ".log")
						{
							continue;
						}
						DateTime InputFileTime = File.GetLastWriteTimeUtc(InputFileName);
						if (InputFileTime.CompareTo(ApkTime) > 0)
						{
							bAllInputsCurrent = false;
							Logger.LogInformation("{DestApkName} is out of date due to newer input file {InputFileName}", DestApkName, InputFileName);
						}
					}
				}
			}

			return bAllInputsCurrent;
		}

		private int ConvertDepthBufferIniValue(string IniValue)
		{
			switch (IniValue.ToLower())
			{
				case "bits16":
					return 16;
				case "bits24":
					return 24;
				case "bits32":
					return 32;
				default:
					return 0;
			}
		}

		private string ConvertOrientationIniValue(string IniValue)
		{
			switch (IniValue.ToLower())
			{
				case "portrait":
					return "portrait";
				case "reverseportrait":
					return "reversePortrait";
				case "sensorportrait":
					return "sensorPortrait";
				case "landscape":
					return "landscape";
				case "reverselandscape":
					return "reverseLandscape";
				case "sensorlandscape":
					return "sensorLandscape";
				case "sensor":
					return "sensor";
				case "fullsensor":
					return "fullSensor";
				default:
					return "landscape";
			}
		}

		private string GetOrientation(string NDKArch)
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string Orientation;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "Orientation", out Orientation);

			// check for UPL override
			string OrientationOverride = UPL!.ProcessPluginNode(NDKArch, "orientationOverride", "");
			if (!String.IsNullOrEmpty(OrientationOverride))
			{
				Orientation = OrientationOverride;
			}

			return ConvertOrientationIniValue(Orientation);
		}

		private void DetermineScreenOrientationRequirements(string Arch, out bool bNeedPortrait, out bool bNeedLandscape)
		{
			bNeedLandscape = false;
			bNeedPortrait = false;

			switch (GetOrientation(Arch).ToLower())
			{
				case "portrait":
					bNeedPortrait = true;
					break;
				case "reverseportrait":
					bNeedPortrait = true;
					break;
				case "sensorportrait":
					bNeedPortrait = true;
					break;

				case "landscape":
					bNeedLandscape = true;
					break;
				case "reverselandscape":
					bNeedLandscape = true;
					break;
				case "sensorlandscape":
					bNeedLandscape = true;
					break;

				case "sensor":
					bNeedPortrait = true;
					bNeedLandscape = true;
					break;
				case "fullsensor":
					bNeedPortrait = true;
					bNeedLandscape = true;
					break;

				default:
					bNeedPortrait = true;
					bNeedLandscape = true;
					break;
			}
		}

		private void PickDownloaderScreenOrientation(string UnrealBuildPath, bool bNeedPortrait, bool bNeedLandscape)
		{
			// Remove unused downloader_progress.xml to prevent missing resource
			if (!bNeedPortrait)
			{
				string LayoutPath = UnrealBuildPath + "/res/layout-port/downloader_progress.xml";
				SafeDeleteFile(LayoutPath);
			}
			if (!bNeedLandscape)
			{
				string LayoutPath = UnrealBuildPath + "/res/layout-land/downloader_progress.xml";
				SafeDeleteFile(LayoutPath);
			}

			// Loop through each of the resolutions (only /res/drawable/ is required, others are optional)
			string[] Resolutions = new string[] { "/res/drawable/", "/res/drawable-ldpi/", "/res/drawable-mdpi/", "/res/drawable-hdpi/", "/res/drawable-xhdpi/" };
			foreach (string ResolutionPath in Resolutions)
			{
				string PortraitFilename = UnrealBuildPath + ResolutionPath + "downloadimagev.png";
				if (bNeedPortrait)
				{
					if (!File.Exists(PortraitFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Logger.LogWarning("Warning: Downloader screen source image {PortraitFilename} not available, downloader screen will not function properly!", PortraitFilename);
					}
				}
				else
				{
					// Remove unused image
					SafeDeleteFile(PortraitFilename);
				}

				string LandscapeFilename = UnrealBuildPath + ResolutionPath + "downloadimageh.png";
				if (bNeedLandscape)
				{
					if (!File.Exists(LandscapeFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Logger.LogWarning("Warning: Downloader screen source image {LandscapeFilename} not available, downloader screen will not function properly!", LandscapeFilename);
					}
				}
				else
				{
					// Remove unused image
					SafeDeleteFile(LandscapeFilename);
				}
			}
		}

		private void PickSplashScreenOrientation(string UnrealBuildPath, bool bNeedPortrait, bool bNeedLandscape)
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			bool bShowLaunchImage = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bShowLaunchImage", out bShowLaunchImage);
			bool bPackageForMetaQuest = IsPackagingForMetaQuest(Ini);

			//override the parameters if we are not showing a launch image or are packaging for Meta Quest
			if (bPackageForMetaQuest || !bShowLaunchImage)
			{
				bNeedPortrait = bNeedLandscape = false;
			}

			// Remove unused styles.xml to prevent missing resource
			if (!bNeedPortrait)
			{
				string StylesPath = UnrealBuildPath + "/res/values-port/styles.xml";
				SafeDeleteFile(StylesPath);
			}
			if (!bNeedLandscape)
			{
				string StylesPath = UnrealBuildPath + "/res/values-land/styles.xml";
				SafeDeleteFile(StylesPath);
			}

			if (bPackageForMetaQuest)
			{
				string LandscapeFilename = UnrealBuildPath + "/res/drawable/" + "splashscreen_landscape.png";
				string OculusSplashTargetPath = UnrealBuildPath + "/assets/vr_splash.png";

				if (bShowLaunchImage)
				{
					if (!File.Exists(LandscapeFilename))
					{
						Logger.LogWarning("Warning: Landscape splash screen source image {0} not available, Oculus splash screen will not function properly!", LandscapeFilename);
					}
					else if (FilesAreDifferent(LandscapeFilename, OculusSplashTargetPath))
					{
						SafeDeleteFile(OculusSplashTargetPath);
						MakeDirectoryIfRequired(OculusSplashTargetPath);
						File.Copy(LandscapeFilename, OculusSplashTargetPath, true);
						Logger.LogInformation("Copying {0} to {1} for Oculus splash", LandscapeFilename, OculusSplashTargetPath);
					}
				}
				else
				{
					// Remove unused image
					SafeDeleteFile(OculusSplashTargetPath);
				}
			}

			// Loop through each of the resolutions (only /res/drawable/ is required, others are optional)
			string[] Resolutions = new string[] { "/res/drawable/", "/res/drawable-ldpi/", "/res/drawable-mdpi/", "/res/drawable-hdpi/", "/res/drawable-xhdpi/" };
			foreach (string ResolutionPath in Resolutions)
			{
				string PortraitFilename = UnrealBuildPath + ResolutionPath + "splashscreen_portrait.png";
				if (bNeedPortrait)
				{
					if (!File.Exists(PortraitFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Logger.LogWarning("Warning: Splash screen source image {PortraitFilename} not available, splash screen will not function properly!", PortraitFilename);
					}
				}
				else
				{
					// Remove unused image
					SafeDeleteFile(PortraitFilename);

					// Remove optional extended resource
					string PortraitXmlFilename = UnrealBuildPath + ResolutionPath + "splashscreen_p.xml";
					SafeDeleteFile(PortraitXmlFilename);
				}

				string LandscapeFilename = UnrealBuildPath + ResolutionPath + "splashscreen_landscape.png";
				if (bNeedLandscape)
				{
					if (!File.Exists(LandscapeFilename) && (ResolutionPath == "/res/drawable/"))
					{
						Logger.LogWarning("Warning: Splash screen source image {LandscapeFilename} not available, splash screen will not function properly!", LandscapeFilename);
					}
				}
				else
				{
					// Remove unused image
					SafeDeleteFile(LandscapeFilename);

					// Remove optional extended resource
					string LandscapeXmlFilename = UnrealBuildPath + ResolutionPath + "splashscreen_l.xml";
					SafeDeleteFile(LandscapeXmlFilename);
				}
			}
		}

		private string? CachedPackageName = null;

		private bool IsLetter(char Input)
		{
			return (Input >= 'A' && Input <= 'Z') || (Input >= 'a' && Input <= 'z');
		}

		private bool IsDigit(char Input)
		{
			return (Input >= '0' && Input <= '9');
		}

		private string GetPackageName(string ProjectName)
		{
			if (CachedPackageName == null)
			{
				ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
				string PackageName;
				Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "PackageName", out PackageName);

				if (PackageName.Contains("[PROJECT]"))
				{
					// project name must start with a letter
					if (!IsLetter(ProjectName[0]))
					{
						throw new BuildException("Package name segments must all start with a letter. Please replace [PROJECT] with a valid name");
					}

					// hyphens not allowed so change them to underscores in project name
					if (ProjectName.Contains('-'))
					{
						Trace.TraceWarning("Project name contained hyphens, converted to underscore");
						ProjectName = ProjectName.Replace("-", "_");
					}

					// check for special characters
					for (int Index = 0; Index < ProjectName.Length; Index++)
					{
						char c = ProjectName[Index];
						if (c != '.' && c != '_' && !IsDigit(c) && !IsLetter(c))
						{
							throw new BuildException("Project name contains illegal characters (only letters, numbers, and underscore allowed); please replace [PROJECT] with a valid name");
						}
					}

					PackageName = PackageName.Replace("[PROJECT]", ProjectName);
				}

				// verify minimum number of segments
				string[] PackageParts = PackageName.Split('.');
				int SectionCount = PackageParts.Length;
				if (SectionCount < 2)
				{
					throw new BuildException("Package name must have at least 2 segments separated by periods (ex. com.projectname, not projectname); please change in Android Project Settings. Currently set to '" + PackageName + "'");
				}

				// hyphens not allowed
				if (PackageName.Contains('-'))
				{
					throw new BuildException("Package names may not contain hyphens; please change in Android Project Settings. Currently set to '" + PackageName + "'");
				}

				// do not allow special characters
				for (int Index = 0; Index < PackageName.Length; Index++)
				{
					char c = PackageName[Index];
					if (c != '.' && c != '_' && !IsDigit(c) && !IsLetter(c))
					{
						throw new BuildException("Package name contains illegal characters (only letters, numbers, and underscore allowed); please change in Android Project Settings. Currently set to '" + PackageName + "'");
					}
				}

				// validate each segment
				for (int Index = 0; Index < SectionCount; Index++)
				{
					if (PackageParts[Index].Length < 1)
					{
						throw new BuildException("Package name segments must have at least one letter; please change in Android Project Settings. Currently set to '" + PackageName + "'");
					}

					if (!IsLetter(PackageParts[Index][0]))
					{
						throw new BuildException("Package name segments must start with a letter; please change in Android Project Settings. Currently set to '" + PackageName + "'");
					}

					// cannot use Java reserved keywords
					foreach (string Keyword in JavaReservedKeywords)
					{
						if (PackageParts[Index] == Keyword)
						{
							throw new BuildException("Package name segments must not be a Java reserved keyword (" + Keyword + "); please change in Android Project Settings. Currently set to '" + PackageName + "'");
						}
					}
				}

				Logger.LogInformation("Using package name: '{PackageName}'", PackageName);
				CachedPackageName = PackageName;
			}

			return CachedPackageName;
		}

		private string GetPublicKey()
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string PlayLicenseKey = "";
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "GooglePlayLicenseKey", out PlayLicenseKey);
			return PlayLicenseKey;
		}

		private bool bHaveReadEngineVersion = false;
		private string EngineMajorVersion = "4";
		private string EngineMinorVersion = "0";
		private string EnginePatchVersion = "0";
		private string EngineChangelist = "0";
		private string? EngineBranch = "UE5";

		private string ReadEngineVersion()
		{
			if (!bHaveReadEngineVersion)
			{
				ReadOnlyBuildVersion Version = ReadOnlyBuildVersion.Current;

				EngineMajorVersion = Version.MajorVersion.ToString();
				EngineMinorVersion = Version.MinorVersion.ToString();
				EnginePatchVersion = Version.PatchVersion.ToString();
				EngineChangelist = Version.Changelist.ToString();
				EngineBranch = Version.BranchName;

				bHaveReadEngineVersion = true;
			}

			return EngineMajorVersion + "." + EngineMinorVersion + "." + EnginePatchVersion;
		}

		private string GenerateManifest(AndroidToolChain ToolChain, string ProjectName, TargetType InTargetType, string EngineDirectory, bool bIsForDistribution, bool bPackageDataInsideApk, string GameBuildFilesPath, bool bHasOBBFiles, bool bDisableVerifyOBBOnStartUp, UnrealArch UnrealArch, string CookFlavor, bool bUseExternalFilesDir, string Configuration, int SDKLevelInt, bool bIsEmbedded, bool bEnableBundle)
		{
			// Read the engine version
			string EngineVersion = ReadEngineVersion();

			int StoreVersion = GetStoreVersion(UnrealArch);

			string Arch = GetNDKArch(UnrealArch);
			int NDKLevelInt = 0;
			int MinSDKVersion = 0;
			int TargetSDKVersion = 0;
			GetMinTargetSDKVersions(ToolChain, UnrealArch, UPL!, Arch, bEnableBundle, out MinSDKVersion, out TargetSDKVersion, out NDKLevelInt);

			// get project version from ini
			ConfigHierarchy GameIni = GetConfigCacheIni(ConfigHierarchyType.Game);
			string ProjectVersion;
			GameIni.GetString("/Script/EngineSettings.GeneralProjectSettings", "ProjectVersion", out ProjectVersion);

			// ini file to get settings from
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			string PackageName = GetPackageName(ProjectName);
			string VersionDisplayName = GetVersionDisplayName(bIsEmbedded);
			string DepthBufferPreference;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "DepthBufferPreference", out DepthBufferPreference);
			float MaxAspectRatioValue;
			if (!Ini.TryGetValue("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MaxAspectRatio", out MaxAspectRatioValue))
			{
				MaxAspectRatioValue = 2.1f;
			}
			string Orientation = ConvertOrientationIniValue(GetOrientation(Arch));
			bool EnableFullScreen;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bFullScreen", out EnableFullScreen);
			bool bUseDisplayCutout;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bUseDisplayCutout", out bUseDisplayCutout);
			bool bAllowResizing;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bAllowResizing", out bAllowResizing);
			bool bSupportSizeChanges;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportSizeChanges", out bSupportSizeChanges);
			bool bRestoreNotificationsOnReboot = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bRestoreNotificationsOnReboot", out bRestoreNotificationsOnReboot);
			List<string>? ExtraManifestNodeTags;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraManifestNodeTags", out ExtraManifestNodeTags);
			List<string>? ExtraApplicationNodeTags;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraApplicationNodeTags", out ExtraApplicationNodeTags);
			List<string>? ExtraActivityNodeTags;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraActivityNodeTags", out ExtraActivityNodeTags);
			string ExtraActivitySettings;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraActivitySettings", out ExtraActivitySettings);
			string ExtraApplicationSettings;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraApplicationSettings", out ExtraApplicationSettings);
			List<string>? ExtraPermissions;
			Ini.GetArray("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "ExtraPermissions", out ExtraPermissions);
			bool bPackageForMetaQuest = IsPackagingForMetaQuest(Ini);
			bool bEnableIAP = false;
			Ini.GetBool("OnlineSubsystemGooglePlay.Store", "bSupportsInAppPurchasing", out bEnableIAP);
			bool bShowLaunchImage = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bShowLaunchImage", out bShowLaunchImage);
			string AndroidGraphicsDebugger;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "AndroidGraphicsDebugger", out AndroidGraphicsDebugger);
			bool bSupportAdMob = true;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportAdMob", out bSupportAdMob);
			bool bValidateTextureFormats;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bValidateTextureFormats", out bValidateTextureFormats);

			bool bBuildForES31 = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForES31", out bBuildForES31);
			bool bSupportsVulkan = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSupportsVulkan", out bSupportsVulkan);

			int PropagateAlpha = 0;
			Ini.GetInt32("/Script/Engine.RendererSettings", "r.Mobile.PropagateAlpha", out PropagateAlpha);

			bool bAllowIMU = true;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bAllowIMU", out bAllowIMU);

			bool bExtractNativeLibs = true;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bExtractNativeLibs", out bExtractNativeLibs);

			bool bPublicLogFiles = true;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bPublicLogFiles", out bPublicLogFiles);
			if (!bUseExternalFilesDir)
			{
				bPublicLogFiles = false;
			}

			string InstallLocation;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "InstallLocation", out InstallLocation);
			switch (InstallLocation.ToLower())
			{
				case "preferexternal":
					InstallLocation = "preferExternal";
					break;
				case "auto":
					InstallLocation = "auto";
					break;
				default:
					InstallLocation = "internalOnly";
					break;
			}

			bool bEnableMulticastSupport = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableMulticastSupport", out bEnableMulticastSupport);

			// only apply density to configChanges if using android-24 or higher and minimum sdk is 17
			bool bAddDensity = (SDKLevelInt >= 24) && (MinSDKVersion >= 17);

			// disable Meta Quest if not supported platform (in this case only arm64 for now)
			if (UnrealArch != UnrealArch.Arm64)
			{
				if (bPackageForMetaQuest)
				{
					Logger.LogInformation("Disabling Package For Meta Quest for unsupported architecture {UnrealArch}", UnrealArch);
					bPackageForMetaQuest = false;
				}
			}

			// disable splash screen for Meta Quest (for now)
			if (bPackageForMetaQuest)
			{
				if (bShowLaunchImage)
				{
					Logger.LogInformation("Disabling Show Launch Image for Meta Quest enabled application");
					bShowLaunchImage = false;
				}
			}

			//figure out the app type
			string AppType = InTargetType == TargetType.Game ? "" : InTargetType.ToString();
			if (CookFlavor.EndsWith("Client"))
			{
				CookFlavor = CookFlavor.Substring(0, CookFlavor.Length - 6);
			}
			if (CookFlavor.EndsWith("Server"))
			{
				CookFlavor = CookFlavor.Substring(0, CookFlavor.Length - 6);
			}

			//figure out which texture compressions are supported
			bool bETC2Enabled, bDXTEnabled, bASTCEnabled;
			bETC2Enabled = bDXTEnabled = bASTCEnabled = false;
			if (CookFlavor.Length < 1)
			{
				//All values supported
				bETC2Enabled = bDXTEnabled = bASTCEnabled = true;
			}
			else
			{
				switch (CookFlavor)
				{
					case "_Multi":
						//need to check ini to determine which are supported
						Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bMultiTargetFormat_ETC2", out bETC2Enabled);
						Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bMultiTargetFormat_DXT", out bDXTEnabled);
						Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bMultiTargetFormat_ASTC", out bASTCEnabled);
						break;
					case "_ETC2":
						bETC2Enabled = true;
						break;
					case "_DXT":
						bDXTEnabled = true;
						break;
					case "_ASTC":
						bASTCEnabled = true;
						break;
					default:
						Logger.LogWarning("Invalid or unknown CookFlavor used in GenerateManifest: {CookFlavor}", CookFlavor);
						break;
				}
			}
			bool bSupportingAllTextureFormats = bETC2Enabled && bDXTEnabled && bASTCEnabled;

			// If it is only ETC2 we need to skip adding the texture format filtering and instead use ES 3.0 as minimum version (it requires ETC2)
			bool bOnlyETC2Enabled = (bETC2Enabled && !(bDXTEnabled || bASTCEnabled));

			string CookedFlavors = (bETC2Enabled ? "ETC2," : "") +
									(bDXTEnabled ? "DXT," : "") +
									(bASTCEnabled ? "ASTC," : "");
			CookedFlavors = (String.IsNullOrEmpty(CookedFlavors)) ? "" : CookedFlavors.Substring(0, CookedFlavors.Length - 1);

			StringBuilder Text = new StringBuilder();
			Text.AppendLine(XML_HEADER);


			bool bIsMakeAAREnabled = false;
			Ini.GetBool("/Script/AndroidSingleInstanceServiceEditor.AndroidSingleInstanceServiceRuntimeSettings", "bEnableASISPlugin", out bIsMakeAAREnabled);

			if (bIsMakeAAREnabled)
			{				
				Text.AppendLine("<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" xmlns:dist=\"http://schemas.android.com/apk/distribution\" xmlns:tools=\"http://schemas.android.com/tools\"");
			}
			else
			{
				Text.AppendLine("<manifest xmlns:android=\"http://schemas.android.com/apk/res/android\" xmlns:tools=\"http://schemas.android.com/tools\"");
			}

			if (ExtraManifestNodeTags != null)
			{
				foreach (string Line in ExtraManifestNodeTags)
				{
					Text.AppendLine("          " + Line);
				}
			}
			Text.AppendLine(String.Format("          android:installLocation=\"{0}\"", InstallLocation));
			Text.AppendLine(String.Format("          android:versionCode=\"{0}\"", StoreVersion));
			Text.AppendLine(String.Format("          android:versionName=\"{0}\">", VersionDisplayName));

			Text.AppendLine("");

			if (TargetSDKVersion >= 30)
			{
				Text.AppendLine("\t<queries>");
				Text.AppendLine("\t\t<intent>");
				Text.AppendLine("\t\t\t<action android:name=\"android.intent.action.VIEW\" />");
				Text.AppendLine("\t\t\t<category android:name=\"android.intent.category.BROWSABLE\" />");
				Text.AppendLine("\t\t\t<data android:scheme=\"http\" />");
				Text.AppendLine("\t\t</intent>");
				Text.AppendLine("\t\t<intent>");
				Text.AppendLine("\t\t\t<action android:name=\"android.intent.action.VIEW\" />");
				Text.AppendLine("\t\t\t<category android:name=\"android.intent.category.BROWSABLE\" />");
				Text.AppendLine("\t\t\t<data android:scheme=\"https\" />");
				Text.AppendLine("\t\t</intent>");
				Text.AppendLine("\t</queries>");
			}

			if (bIsMakeAAREnabled)
			{
				Text.AppendLine("\t<dist:module dist:instant=\"true\" />");
				Text.AppendLine("\t<queries>");
				Text.AppendLine(string.Format("\t\t<package android:name=\"{0}\" />", PackageName));
				Text.AppendLine("\t</queries>");
			}

			Text.AppendLine("\t<!-- Application Definition -->");
			Text.AppendLine("\t<application android:label=\"@string/app_name\"");
			Text.AppendLine("\t             android:icon=\"@drawable/icon\"");

			AndroidToolChain.ClangSanitizer Sanitizer = ToolChain.BuildWithSanitizer();
			// hwasan on NDK r26b+ requires wrap.sh that needs to be unpacked
			if ((Sanitizer != AndroidToolChain.ClangSanitizer.None && (Sanitizer != AndroidToolChain.ClangSanitizer.HwAddress || ToolChain.HasEmbeddedHWASanSupport())) || bEnableScudoMemoryTracing)
			{
				bExtractNativeLibs = true;
			}

			bool bRequestedLegacyExternalStorage = false;
			if (ExtraApplicationNodeTags != null)
			{
				foreach (string Line in ExtraApplicationNodeTags)
				{
					if (Line.Contains("requestLegacyExternalStorage"))
					{
						bRequestedLegacyExternalStorage = true;
					}
					Text.AppendLine("\t             " + Line);
				}
			}
			Text.AppendLine("\t             android:hardwareAccelerated=\"true\"");
			Text.AppendLine(String.Format("\t             android:extractNativeLibs=\"{0}\"", bExtractNativeLibs ? "true" : "false"));
			Text.AppendLine("\t				android:name=\"com.epicgames.unreal.GameApplication\"");
			if (!bIsForDistribution && SDKLevelInt >= 29 && !bRequestedLegacyExternalStorage)
			{
				// work around scoped storage for non-distribution for SDK 29; add to ExtraApplicationNodeTags if you need it for distribution
				Text.AppendLine("\t				android:requestLegacyExternalStorage=\"true\"");
			}
			Text.AppendLine("\t             android:hasCode=\"true\">");
			if (bShowLaunchImage)
			{
				// normal application settings
				Text.AppendLine("\t\t<activity android:name=\"com.epicgames.unreal.SplashActivity\"");
				Text.AppendLine("\t\t          android:exported=\"true\"");
				Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
				Text.AppendLine("\t\t          android:theme=\"@style/UnrealSplashTheme\"");
				Text.AppendLine("\t\t          android:launchMode=\"singleTask\"");
				if (SDKLevelInt >= 24)
				{
					Text.AppendLine("\t\t          android:resizeableActivity=\"{0}\"", bAllowResizing ? "true" : "false");
				}
				Text.AppendLine(String.Format("\t\t          android:screenOrientation=\"{0}\"", Orientation));
				Text.AppendLine(String.Format("\t\t          android:debuggable=\"{0}\">", bIsForDistribution ? "false" : "true"));
				Text.AppendLine("\t\t\t<intent-filter>");
				Text.AppendLine("\t\t\t\t<action android:name=\"android.intent.action.MAIN\" />");
				Text.AppendLine(String.Format("\t\t\t\t<category android:name=\"android.intent.category.LAUNCHER\" />"));
				Text.AppendLine("\t\t\t</intent-filter>");
				Text.AppendLine("\t\t</activity>");
				Text.AppendLine("\t\t<activity android:name=\"com.epicgames.unreal.GameActivity\"");
				Text.AppendLine("\t\t          android:exported=\"true\"");
				Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
				Text.AppendLine("\t\t          android:theme=\"@style/UnrealSplashTheme\"");
				Text.AppendLine(bAddDensity ? "\t\t          android:configChanges=\"mcc|mnc|uiMode|density|screenSize|smallestScreenSize|screenLayout|orientation|keyboardHidden|keyboard|navigation|touchscreen|locale|fontScale|layoutDirection\""
											: "\t\t          android:configChanges=\"mcc|mnc|uiMode|screenSize|smallestScreenSize|screenLayout|orientation|keyboardHidden|keyboard|navigation|touchscreen|locale|fontScale|layoutDirection\"");
			}
			else
			{
				Text.AppendLine("\t\t<activity android:name=\"com.epicgames.unreal.GameActivity\"");
				Text.AppendLine("\t\t          android:exported=\"true\"");
				Text.AppendLine("\t\t          android:label=\"@string/app_name\"");
				Text.AppendLine("\t\t          android:theme=\"@android:style/Theme.Black.NoTitleBar.Fullscreen\"");
				Text.AppendLine(bAddDensity ? "\t\t          android:configChanges=\"mcc|mnc|uiMode|density|screenSize|smallestScreenSize|screenLayout|orientation|keyboardHidden|keyboard|navigation|touchscreen|locale|fontScale|layoutDirection\""
											: "\t\t          android:configChanges=\"mcc|mnc|uiMode|screenSize|smallestScreenSize|screenLayout|orientation|keyboardHidden|keyboard|navigation|touchscreen|locale|fontScale|layoutDirection\"");

			}
			if (SDKLevelInt >= 24)
			{
				Text.AppendLine("\t\t          android:resizeableActivity=\"{0}\"", bAllowResizing ? "true" : "false");
			}
			Text.AppendLine("\t\t          android:launchMode=\"singleTask\"");
			Text.AppendLine(String.Format("\t\t          android:screenOrientation=\"{0}\"", Orientation));
			if (ExtraActivityNodeTags != null)
			{
				foreach (string Line in ExtraActivityNodeTags)
				{
					Text.AppendLine("\t\t          " + Line);
				}
			}
			Text.AppendLine(String.Format("\t\t          android:debuggable=\"{0}\">", bIsForDistribution ? "false" : "true"));
			Text.AppendLine("\t\t\t<meta-data android:name=\"android.app.lib_name\" android:value=\"Unreal\"/>");
			if (!bShowLaunchImage)
			{
				Text.AppendLine("\t\t\t<intent-filter>");
				Text.AppendLine("\t\t\t\t<action android:name=\"android.intent.action.MAIN\" />");
				Text.AppendLine(String.Format("\t\t\t\t<category android:name=\"android.intent.category.LAUNCHER\" />"));
				Text.AppendLine("\t\t\t</intent-filter>");
			}
			if (!String.IsNullOrEmpty(ExtraActivitySettings))
			{
				ExtraActivitySettings = ExtraActivitySettings.Replace("\\n", "\n");
				foreach (string Line in ExtraActivitySettings.Split("\r\n".ToCharArray()))
				{
					Text.AppendLine("\t\t\t" + Line);
				}
			}
			string ActivityAdditionsFile = Path.Combine(GameBuildFilesPath, "ManifestActivityAdditions.txt");
			if (File.Exists(ActivityAdditionsFile))
			{
				foreach (string Line in File.ReadAllLines(ActivityAdditionsFile))
				{
					Text.AppendLine("\t\t\t" + Line);
				}
			}
			Text.AppendLine("\t\t</activity>");

			// For OBB download support
			if (bShowLaunchImage)
			{
				Text.AppendLine("\t\t<activity android:name=\".DownloaderActivity\"");
				Text.AppendLine(String.Format("\t\t          android:screenOrientation=\"{0}\"", Orientation));
				Text.AppendLine(bAddDensity ? "\t\t          android:configChanges=\"mcc|mnc|uiMode|density|screenSize|orientation|keyboardHidden|keyboard\""
											: "\t\t          android:configChanges=\"mcc|mnc|uiMode|screenSize|orientation|keyboardHidden|keyboard\"");
				Text.AppendLine("\t\t          android:theme=\"@style/UnrealSplashTheme\" />");
			}
			else
			{
				Text.AppendLine("\t\t<activity android:name=\".DownloaderActivity\" />");
			}

			// Figure out the required startup permissions if targetting devices supporting runtime permissions
			String StartupPermissions = "";
			if (TargetSDKVersion >= 23)
			{
				if (Configuration != "Shipping" || !bUseExternalFilesDir)
				{
					StartupPermissions = StartupPermissions + (StartupPermissions.Length > 0 ? "," : "") + "android.permission.WRITE_EXTERNAL_STORAGE";
				}
			}

			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.EngineVersion\" android:value=\"{0}\"/>", EngineVersion));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.EngineBranch\" android:value=\"{0}\"/>", EngineBranch));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.ProjectVersion\" android:value=\"{0}\"/>", ProjectVersion));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.DepthBufferPreference\" android:value=\"{0}\"/>", ConvertDepthBufferIniValue(DepthBufferPreference)));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bPackageDataInsideApk\" android:value=\"{0}\"/>", bPackageDataInsideApk ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bVerifyOBBOnStartUp\" android:value=\"{0}\"/>", (bIsForDistribution && !bDisableVerifyOBBOnStartUp) ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bShouldHideUI\" android:value=\"{0}\"/>", EnableFullScreen ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.ProjectName\" android:value=\"{0}\"/>", ProjectName));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.AppType\" android:value=\"{0}\"/>", AppType));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bHasOBBFiles\" android:value=\"{0}\"/>", bHasOBBFiles ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.BuildConfiguration\" android:value=\"{0}\"/>", Configuration));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.CookedFlavors\" android:value=\"{0}\"/>", CookedFlavors));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bValidateTextureFormats\" android:value=\"{0}\"/>", bValidateTextureFormats ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bUseExternalFilesDir\" android:value=\"{0}\"/>", bUseExternalFilesDir ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bPublicLogFiles\" android:value=\"{0}\"/>", bPublicLogFiles ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bUseDisplayCutout\" android:value=\"{0}\"/>", bUseDisplayCutout ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bAllowIMU\" android:value=\"{0}\"/>", bAllowIMU ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.bSupportsVulkan\" android:value=\"{0}\"/>", bSupportsVulkan ? "true" : "false"));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.PropagateAlpha\" android:value=\"{0}\"/>", PropagateAlpha));
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"com.epicgames.unreal.GameActivity.StartupPermissions\" android:value=\"{0}\"/>", StartupPermissions));
			if (TargetSDKVersion >= 29)
			{
				Text.AppendLine(String.Format("\t\t<meta-data android:name=\"android.supports_size_changes\" android:value=\"{0}\"/>", bSupportSizeChanges ? "true" : "false"));
			}
			Text.AppendLine("\t\t<meta-data android:name=\"com.google.android.gms.games.APP_ID\"");
			Text.AppendLine("\t\t           android:value=\"@string/app_id\" />");
			Text.AppendLine("\t\t<meta-data android:name=\"com.google.android.gms.version\"");
			Text.AppendLine("\t\t           android:value=\"@integer/google_play_services_version\" />");
			if (bSupportAdMob)
			{
				Text.AppendLine("\t\t<activity android:name=\"com.google.android.gms.ads.AdActivity\"");
				Text.AppendLine("\t\t          android:configChanges=\"keyboard|keyboardHidden|orientation|screenLayout|uiMode|screenSize|smallestScreenSize\"/>");
			}
			if (!String.IsNullOrEmpty(ExtraApplicationSettings))
			{
				ExtraApplicationSettings = ExtraApplicationSettings.Replace("\\n", "\n");
				foreach (string Line in ExtraApplicationSettings.Split("\r\n".ToCharArray()))
				{
					Text.AppendLine("\t\t" + Line);
				}
			}
			string ApplicationAdditionsFile = Path.Combine(GameBuildFilesPath, "ManifestApplicationAdditions.txt");
			if (File.Exists(ApplicationAdditionsFile))
			{
				foreach (string Line in File.ReadAllLines(ApplicationAdditionsFile))
				{
					Text.AppendLine("\t\t" + Line);
				}
			}

			// Declare the 8 OpenGL program compiling services.
			Text.AppendLine(@"		<service android:name=""com.epicgames.unreal.psoservices.OGLProgramService"" android:process="":psoprogramservice"" />");
			// Declare the remaining 7 OpenGL program compiling services. (all derived from OGLProgramService)
			for (int i = 1; i < 8; i++)
			{
				String serviceLine = String.Format("		<service android:name=\"com.epicgames.unreal.psoservices.OGLProgramService{0}\" android:process=\":psoprogramservice{0}\" />", i);
				Text.AppendLine(serviceLine);
			}

			// Declare the 8 Vulkan program compiling services.
			Text.AppendLine(@"		<service android:name=""com.epicgames.unreal.psoservices.VulkanProgramService"" android:process="":psoprogramservice"" />");
			// Declare the remaining 7 Vulkan program compiling services. (all derived from VulkanProgramService)
			for (int i = 1; i < 8; i++)
			{
				String serviceLine = String.Format("		<service android:name=\"com.epicgames.unreal.psoservices.VulkanProgramService{0}\" android:process=\":psoprogramservice{0}\" />", i);
				Text.AppendLine(serviceLine);
			}

			// Required for OBB download support
			Text.AppendLine("\t\t<service android:name=\"OBBDownloaderService\" />");
			Text.AppendLine("\t\t<receiver android:name=\"AlarmReceiver\" android:exported=\"false\"/>");

			Text.AppendLine("\t\t<receiver android:name=\"com.epicgames.unreal.LocalNotificationReceiver\" android:exported=\"false\"/>");
			Text.AppendLine("\t\t<receiver android:name=\"com.epicgames.unreal.CellularReceiver\" android:exported=\"false\"/>");

			if (bRestoreNotificationsOnReboot)
			{
				Text.AppendLine("\t\t<receiver android:name=\"com.epicgames.unreal.BootCompleteReceiver\" android:exported=\"true\">");
				Text.AppendLine("\t\t\t<intent-filter>");
				Text.AppendLine("\t\t\t\t<action android:name=\"android.intent.action.BOOT_COMPLETED\" />");
				Text.AppendLine("\t\t\t\t<action android:name=\"android.intent.action.QUICKBOOT_POWERON\" />");
				Text.AppendLine("\t\t\t\t<action android:name=\"com.htc.intent.action.QUICKBOOT_POWERON\" />");
				Text.AppendLine("\t\t\t</intent-filter>");
				Text.AppendLine("\t\t</receiver>");
			}

			Text.AppendLine("\t\t<receiver android:name=\"com.epicgames.unreal.MulticastBroadcastReceiver\" android:exported=\"true\">");
			Text.AppendLine("\t\t\t<intent-filter>");
			Text.AppendLine("\t\t\t\t<action android:name=\"com.android.vending.INSTALL_REFERRER\" />");
			Text.AppendLine("\t\t\t</intent-filter>");
			Text.AppendLine("\t\t</receiver>");

			// Max supported aspect ratio
			string MaxAspectRatioString = MaxAspectRatioValue.ToString("f", System.Globalization.CultureInfo.InvariantCulture);
			Text.AppendLine(String.Format("\t\t<meta-data android:name=\"android.max_aspect\" android:value=\"{0}\" />", MaxAspectRatioString));

			Text.AppendLine("\t</application>");

			Text.AppendLine("");
			Text.AppendLine("\t<!-- Requirements -->");

			// check for an override for the requirements section of the manifest
			string RequirementsOverrideFile = Path.Combine(GameBuildFilesPath, "ManifestRequirementsOverride.txt");
			if (File.Exists(RequirementsOverrideFile))
			{
				foreach (string Line in File.ReadAllLines(RequirementsOverrideFile))
				{
					Text.AppendLine("\t" + Line);
				}
			}
			else
			{
				Text.AppendLine("\t<uses-feature android:glEsVersion=\"" + AndroidToolChain.GetGLESVersion(bBuildForES31) + "\" android:required=\"true\" />");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.INTERNET\"/>");
				if (Configuration != "Shipping" || !bUseExternalFilesDir)
				{
					Text.AppendLine("\t<uses-permission android:name=\"android.permission.WRITE_EXTERNAL_STORAGE\"/>");
				}
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.ACCESS_NETWORK_STATE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.WAKE_LOCK\"/>");
				//	Text.AppendLine("\t<uses-permission android:name=\"android.permission.READ_PHONE_STATE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"com.android.vending.CHECK_LICENSE\"/>");
				Text.AppendLine("\t<uses-permission android:name=\"android.permission.ACCESS_WIFI_STATE\"/>");

				if (bEnableMulticastSupport)
				{
					// This permission is needed to be able to acquire a WifiManager.MulticastLock so broadcast/multcast traffic is 
					// not filtered out by the device network interface 
					Text.AppendLine("\t<uses-permission android:name=\"android.permission.CHANGE_WIFI_MULTICAST_STATE\"/>");
				}

				if (bRestoreNotificationsOnReboot)
				{
					Text.AppendLine("\t<uses-permission android:name=\"android.permission.RECEIVE_BOOT_COMPLETED\"/>");
				}

				if(!bPackageForMetaQuest)
				{
					Text.AppendLine("\t<uses-permission android:name=\"android.permission.MODIFY_AUDIO_SETTINGS\"/>");
					Text.AppendLine("\t<uses-permission android:name=\"android.permission.VIBRATE\"/>");
				}

				//			Text.AppendLine("\t<uses-permission android:name=\"android.permission.DISABLE_KEYGUARD\"/>");

				if (bEnableIAP)
				{
					Text.AppendLine("\t<uses-permission android:name=\"com.android.vending.BILLING\"/>");
				}
				if (ExtraPermissions != null)
				{
					foreach (string Permission in ExtraPermissions)
					{
						string TrimmedPermission = Permission.Trim(' ');
						if (!String.IsNullOrEmpty(TrimmedPermission))
						{
							string PermissionString = String.Format("\t<uses-permission android:name=\"{0}\"/>", TrimmedPermission);
							if (!Text.ToString().Contains(PermissionString))
							{
								Text.AppendLine(PermissionString);
							}
						}
					}
				}
				string RequirementsAdditionsFile = Path.Combine(GameBuildFilesPath, "ManifestRequirementsAdditions.txt");
				if (File.Exists(RequirementsAdditionsFile))
				{
					foreach (string Line in File.ReadAllLines(RequirementsAdditionsFile))
					{
						Text.AppendLine("\t" + Line);
					}
				}
				if (AndroidGraphicsDebugger.ToLower() == "adreno")
				{
					string PermissionString = "\t<uses-permission android:name=\"com.qti.permission.PROFILER\"/>";
					if (!Text.ToString().Contains(PermissionString))
					{
						Text.AppendLine(PermissionString);
					}
				}

				if (!bSupportingAllTextureFormats)
				{
					Text.AppendLine("\t<!-- Supported texture compression formats (cooked) -->");
					if (bETC2Enabled && !bOnlyETC2Enabled)
					{
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_COMPRESSED_RGB8_ETC2\" />");
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_COMPRESSED_RGBA8_ETC2_EAC\" />");
					}
					if (bDXTEnabled)
					{
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_EXT_texture_compression_dxt1\" />");
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_EXT_texture_compression_s3tc\" />");
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_NV_texture_compression_s3tc\" />");
					}
					if (bASTCEnabled)
					{
						Text.AppendLine("\t<supports-gl-texture android:name=\"GL_KHR_texture_compression_astc_ldr\" />");
					}
				}
			}

			Text.AppendLine("</manifest>");

			// allow plugins to modify final manifest HERE
			XDocument XDoc;
			try
			{
				XDoc = XDocument.Parse(Text.ToString());
			}
			catch (Exception e)
			{
				throw new BuildException("AndroidManifest.xml is invalid {0}\n{1}", e, Text.ToString());
			}

			UPL!.ProcessPluginNode(Arch, "androidManifestUpdates", "", ref XDoc);
			return XDoc.ToString();
		}

		private string GenerateProguard(string Arch, string EngineSourcePath, string GameBuildFilesPath)
		{
			StringBuilder Text = new StringBuilder();

			string ProguardFile = Path.Combine(EngineSourcePath, "proguard-project.txt");
			if (File.Exists(ProguardFile))
			{
				foreach (string Line in File.ReadAllLines(ProguardFile))
				{
					Text.AppendLine(Line);
				}
			}

			string ProguardAdditionsFile = Path.Combine(GameBuildFilesPath, "ProguardAdditions.txt");
			if (File.Exists(ProguardAdditionsFile))
			{
				foreach (string Line in File.ReadAllLines(ProguardAdditionsFile))
				{
					Text.AppendLine(Line);
				}
			}

			// add plugin additions
			return UPL!.ProcessPluginNode(Arch, "proguardAdditions", Text.ToString());
		}

		private void ValidateGooglePlay(string UnrealBuildPath)
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			bool bEnableGooglePlaySupport;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableGooglePlaySupport", out bEnableGooglePlaySupport);

			if (!bEnableGooglePlaySupport)
			{
				// do not need to do anything; it is fine
				return;
			}

			string IniAppId;
			bool bInvalidIniAppId = false;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "GamesAppID", out IniAppId);

			//validate the value found in the AndroidRuntimeSettings
			Int64 Value;
			if (IniAppId.Length == 0 || !Int64.TryParse(IniAppId, out Value))
			{
				bInvalidIniAppId = true;
			}

			bool bInvalid = false;
			string ReplacementId = "";
			String Filename = Path.Combine(UnrealBuildPath, "res", "values", "GooglePlayAppID.xml");
			if (File.Exists(Filename))
			{
				string[] FileContent = File.ReadAllLines(Filename);
				int LineIndex = -1;
				foreach (string Line in FileContent)
				{
					++LineIndex;

					int StartIndex = Line.IndexOf("\"app_id\">");
					if (StartIndex < 0)
					{
						continue;
					}

					StartIndex += 9;
					int EndIndex = Line.IndexOf("</string>");
					if (EndIndex < 0)
					{
						continue;
					}

					string XmlAppId = Line.Substring(StartIndex, EndIndex - StartIndex);

					//validate that the AppId matches the .ini value for the GooglePlay AppId, assuming it's valid
					if (!bInvalidIniAppId && IniAppId.CompareTo(XmlAppId) != 0)
					{
						Logger.LogInformation("Replacing Google Play AppID in GooglePlayAppID.xml with AndroidRuntimeSettings .ini value");

						bInvalid = true;
						ReplacementId = IniAppId;

					}
					else if (XmlAppId.Length == 0 || !Int64.TryParse(XmlAppId, out Value))
					{
						Logger.LogWarning("\nWARNING: GooglePlay Games App ID is invalid! Replacing it with \"1\"");

						//write file with something which will fail but not cause an exception if executed
						bInvalid = true;
						ReplacementId = "1";
					}

					if (bInvalid)
					{
						// remove any read only flags if invalid so it can be replaced
						FileInfo DestFileInfo = new FileInfo(Filename);
						DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;

						//preserve the rest of the file, just fix up this line
						string NewLine = Line.Replace("\"app_id\">" + XmlAppId + "</string>", "\"app_id\">" + ReplacementId + "</string>");
						FileContent[LineIndex] = NewLine;

						File.WriteAllLines(Filename, FileContent);
					}

					break;
				}
			}
			else
			{
				string NewAppId;
				// if we don't have an appID to use from the config, write file with something which will fail but not cause an exception if executed
				if (bInvalidIniAppId)
				{
					Logger.LogWarning("\nWARNING: Creating GooglePlayAppID.xml using a Google Play AppID of \"1\" because there was no valid AppID in AndroidRuntimeSettings!");
					NewAppId = "1";
				}
				else
				{
					Logger.LogInformation("Creating GooglePlayAppID.xml with AndroidRuntimeSettings .ini value");
					NewAppId = IniAppId;
				}

				File.WriteAllText(Filename, XML_HEADER + "\n<resources>\n\t<string name=\"app_id\">" + NewAppId + "</string>\n</resources>\n");
			}
		}

		private bool FilesAreDifferent(string SourceFilename, string DestFilename)
		{
			// source must exist
			FileInfo SourceInfo = new FileInfo(SourceFilename);
			if (!SourceInfo.Exists)
			{
				throw new BuildException("Can't make an APK without file [{0}]", SourceFilename);
			}

			// different if destination doesn't exist
			FileInfo DestInfo = new FileInfo(DestFilename);
			if (!DestInfo.Exists)
			{
				return true;
			}

			// file lengths differ?
			if (SourceInfo.Length != DestInfo.Length)
			{
				return true;
			}

			// validate timestamps
			TimeSpan Diff = DestInfo.LastWriteTimeUtc - SourceInfo.LastWriteTimeUtc;
			if (Diff.TotalSeconds < -1 || Diff.TotalSeconds > 1)
			{
				return true;
			}

			// could check actual bytes just to be sure, but good enough
			return false;
		}

		private bool FilesAreIdentical(string SourceFilename, string DestFilename)
		{
			// source must exist
			FileInfo SourceInfo = new FileInfo(SourceFilename);
			if (!SourceInfo.Exists)
			{
				throw new BuildException("Can't make an APK without file [{0}]", SourceFilename);
			}

			// different if destination doesn't exist
			FileInfo DestInfo = new FileInfo(DestFilename);
			if (!DestInfo.Exists)
			{
				return false;
			}

			// file lengths differ?
			if (SourceInfo.Length != DestInfo.Length)
			{
				return false;
			}

			using (FileStream SourceStream = new FileStream(SourceFilename, FileMode.Open, FileAccess.Read, FileShare.Read))
			using (FileStream DestStream = new FileStream(DestFilename, FileMode.Open, FileAccess.Read, FileShare.Read))
			{
				using (BinaryReader SourceReader = new BinaryReader(SourceStream))
				using (BinaryReader DestReader = new BinaryReader(DestStream))
				{
					bool bEOF = false;
					while (!bEOF)
					{
						byte[] SourceData = SourceReader.ReadBytes(32768);
						if (SourceData.Length == 0)
						{
							bEOF = true;
							break;
						}

						byte[] DestData = DestReader.ReadBytes(32768);
						if (!SourceData.SequenceEqual(DestData))
						{
							return false;
						}
					}
					return true;
				}
			}
		}

		private bool RequiresOBB(bool bDisallowPackageInAPK, string OBBLocation)
		{
			if (bDisallowPackageInAPK)
			{
				Logger.LogInformation("APK contains data.");
				return false;
			}
			else if (!String.IsNullOrEmpty(Environment.GetEnvironmentVariable("uebp_LOCAL_ROOT")))
			{
				Logger.LogInformation("On build machine.");
				return true;
			}
			else
			{
				Logger.LogInformation("Looking for OBB.");
				return File.Exists(OBBLocation);
			}
		}

		private bool CreateRunGradle(string GradlePath)
		{
			string RunGradleBatFilename = Path.Combine(GradlePath, "rungradle.bat");

			// check for an unused drive letter
			string UnusedDriveLetter = "";
			bool bFound = true;
			DriveInfo[] AllDrives = DriveInfo.GetDrives();
			for (char DriveLetter = 'Z'; DriveLetter >= 'A'; DriveLetter--)
			{
				UnusedDriveLetter = Char.ToString(DriveLetter) + ":";
				bFound = false;
				for (int DriveIndex = AllDrives.Length - 1; DriveIndex >= 0; DriveIndex--)
				{
					if (AllDrives[DriveIndex].Name.ToUpper().StartsWith(UnusedDriveLetter))
					{
						bFound = true;
						break;
					}
				}
				if (!bFound)
				{
					break;
				}
			}

			if (bFound)
			{
				Logger.LogInformation("\nUnable to apply subst, using gradlew.bat directly (all drive letters in use!)");
				return false;
			}

			Logger.LogInformation("\nCreating rungradle.bat to work around commandline length limit (using unused drive letter {UnusedDriveLetter})", UnusedDriveLetter);

			// make sure rungradle.bat isn't read-only
			if (File.Exists(RunGradleBatFilename))
			{
				FileAttributes Attribs = File.GetAttributes(RunGradleBatFilename);
				if (Attribs.HasFlag(FileAttributes.ReadOnly))
				{
					File.SetAttributes(RunGradleBatFilename, Attribs & ~FileAttributes.ReadOnly);
				}
			}

			// generate new rungradle.bat with an unused drive letter for subst
			string RunGradleBatText =
					"@echo off\n" +
					"setlocal\n" +
					"set GRADLEPATH=%~dp0\n" +
					"set GRADLE_CMD_LINE_ARGS=\n" +
					":setupArgs\n" +
					"if \"\"%1\"\"==\"\"\"\" goto doneStart\n" +
					"set GRADLE_CMD_LINE_ARGS=%GRADLE_CMD_LINE_ARGS% %1\n" +
					"shift\n" +
					"goto setupArgs\n\n" +
					":doneStart\n" +
					"subst " + UnusedDriveLetter + " \"%CD%\"\n" +
					"pushd " + UnusedDriveLetter + "\n" +
					"call \"%GRADLEPATH%\\gradlew.bat\" %GRADLE_CMD_LINE_ARGS%\n" +
					"set GRADLEERROR=%ERRORLEVEL%\n" +
					"popd\n" +
					"subst " + UnusedDriveLetter + " /d\n" +
					"exit /b %GRADLEERROR%\n";

			File.WriteAllText(RunGradleBatFilename, RunGradleBatText);

			return true;
		}

		private bool BundleEnabled()
		{
			if (ForceAPKGeneration)
			{
				return false;
			}
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			bool bEnableBundle = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableBundle", out bEnableBundle);
			return bEnableBundle;
		}

		private bool IsLicenseAgreementValid()
		{
			string LicensePath = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%/licenses");

			// directory must exist
			if (!Directory.Exists(LicensePath))
			{
				Logger.LogInformation("Directory doesn't exist {LicensePath}", LicensePath);
				return false;
			}

			// license file must exist
			string LicenseFilename = Path.Combine(LicensePath, "android-sdk-license");
			if (!File.Exists(LicenseFilename))
			{
				Logger.LogInformation("File doesn't exist {LicenseFilename}", LicenseFilename);
				return false;
			}

			// ignore contents of hash for now (Gradle will report if it isn't valid)
			return true;
		}

		private void GetMinTargetSDKVersions(AndroidToolChain ToolChain, UnrealArch Arch, UnrealPluginLanguage UPL, string NDKArch, bool bEnableBundle, out int MinSDKVersion, out int TargetSDKVersion, out int NDKLevelInt)
		{
			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MinSDKVersion", out MinSDKVersion);
			TargetSDKVersion = MinSDKVersion;
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "TargetSDKVersion", out TargetSDKVersion);

			// Check for targetSDKOverride from UPL
			string TargetOverride = UPL.ProcessPluginNode(NDKArch, "targetSDKOverride", "");
			if (!String.IsNullOrEmpty(TargetOverride))
			{
				int OverrideInt = 0;
				if (Int32.TryParse(TargetOverride, out OverrideInt))
				{
					TargetSDKVersion = OverrideInt;
				}
			}

			if ((ToolChain.BuildWithSanitizer() != AndroidToolChain.ClangSanitizer.None) && (MinSDKVersion < 27))
			{
				MinSDKVersion = 27;
				Logger.LogInformation("Fixing minSdkVersion; requires minSdkVersion of {MinVer} for Clang's Sanitizers", MinSDKVersion);
			}

			if (bEnableBundle && MinSDKVersion < MinimumSDKLevelForBundle)
			{
				MinSDKVersion = MinimumSDKLevelForBundle;
				Logger.LogInformation("Fixing minSdkVersion; requires minSdkVersion of {MinVer} for App Bundle support", MinimumSDKLevelForBundle);
			}

			// Make sure minSdkVersion is at least 13 (need this for appcompat-v13 used by AndroidPermissions)
			// this may be changed by active plugins (Google Play Services 11.0.4 needs 14 for example)
			if (MinSDKVersion < MinimumSDKLevelForGradle)
			{
				MinSDKVersion = MinimumSDKLevelForGradle;
				Logger.LogInformation("Fixing minSdkVersion; requires minSdkVersion of {MinVer} with Gradle based on active plugins", MinimumSDKLevelForGradle);
			}

			// Get NDK API level and enforce minimum
			NDKLevelInt = ToolChain.GetNdkApiLevelInt();
			if (NDKLevelInt < AndroidToolChain.MinimumNDKAPILevel)
			{
				// 21 is required for GL ES3.1, 26 for ANativeWindow_setBuffersTransform
				NDKLevelInt = AndroidToolChain.MinimumNDKAPILevel;
			}

			// fix up the MinSdkVersion to be at least NDKLevelInt
			if (MinSDKVersion < NDKLevelInt)
			{
				Logger.LogInformation("Fixing minSdkVersion; NDK level is {NDKLevelInt} which is above minSdkVersion {MinSDKVersion}.", NDKLevelInt, MinSDKVersion);
				MinSDKVersion = NDKLevelInt;
			}

			// fix up the TargetSDK to be at least MinSdkVersion
			if (TargetSDKVersion < MinSDKVersion)
			{
				Logger.LogInformation("Fixing targetSdkVersion; minSdkVersion is {MinSDKVersion} which is above targetSdkVersion {TargetSDKVersion}.", MinSDKVersion, TargetSDKVersion);
				TargetSDKVersion = MinSDKVersion;
			}
		}

		private uint[] CRCTablesSB8 = {
			0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
			0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7, 0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
			0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
			0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
			0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433, 0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
			0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
			0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
			0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f, 0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
			0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
			0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
			0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b, 0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
			0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
			0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
			0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777, 0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
			0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
			0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
		};

		private bool CopyAPKAndReplaceSO(ILogger Logger, string SourceAPK, string DestAPK, string SourceSOFile, string DestSOFile)
		{
			BinaryReader SourceReader;
			BinaryWriter DestWriter;
			BinaryWriter DirectoryWriter;

			string DirectoryFilename = DestAPK + ".dir";

			try
			{
				SourceReader = new BinaryReader(new FileStream(SourceAPK, FileMode.Open, FileAccess.Read));
			}
			catch (IOException)
			{
				return false;
			}

			try
			{
				DestWriter = new BinaryWriter(new FileStream(DestAPK, FileMode.Create));
			}
			catch (IOException)
			{
				return false;
			}

			try
			{
				DirectoryWriter = new BinaryWriter(new FileStream(DirectoryFilename, FileMode.Create));
			}
			catch (IOException)
			{
				return false;
			}

			byte[] CopyBuffer = new byte[65536];
			ushort TotalEntries = 0;
			uint Remaining;

			uint Signature = 0x02014b50;
			ushort DirVersion = 0;
			ushort DiskNumber = 0;
			ushort DiskStart = 0;
			ushort InternalAttr = 0;
			uint ExternalAttr = 0;
			ushort CommentLen = 0;

			// copy files
			while (true)
			{
				uint Header = SourceReader.ReadUInt32();
				if (Header != 0x04034b50)
				{
					SourceReader.Close();
					break;
				}

				ushort Version = SourceReader.ReadUInt16();
				ushort Flags = SourceReader.ReadUInt16();
				ushort Compression = SourceReader.ReadUInt16();
				ushort ModTime = SourceReader.ReadUInt16();
				ushort ModDate = SourceReader.ReadUInt16();
				uint CRC32Value = SourceReader.ReadUInt32();
				uint CompressedSize = SourceReader.ReadUInt32();
				uint UncompressedSize = SourceReader.ReadUInt32();
				ushort FilenameLen = SourceReader.ReadUInt16();
				ushort ExtraLen = SourceReader.ReadUInt16();
				byte[] Filename = new byte[FilenameLen];
				SourceReader.BaseStream.Read(Filename, 0, FilenameLen);
				byte[] Extra = new byte[ExtraLen];
				SourceReader.BaseStream.Read(Extra, 0, ExtraLen);

				string FilenameStr = Encoding.UTF8.GetString(Filename, 0, FilenameLen);
				if (!FilenameStr.Equals(DestSOFile))
				{
					TotalEntries++;
					uint Location = (uint)DestWriter.BaseStream.Position;

					// calculate location after header and any additional alignment needed (only if uncompressed)
					uint HeaderSize = (uint)(4 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + FilenameLen + ExtraLen);
					uint HeaderEnd = Location + HeaderSize;
					uint DataStart = (uint)((HeaderEnd + 3) & ~3);
					ushort ExtraAlign = (ushort)(Compression != 0 ? 0 : (DataStart - HeaderEnd));

					// write local header
					DestWriter.Write(Header);
					DestWriter.Write(Version);
					DestWriter.Write(Flags);
					DestWriter.Write(Compression);
					DestWriter.Write(ModTime);
					DestWriter.Write(ModDate);
					DestWriter.Write(CRC32Value);
					DestWriter.Write(CompressedSize);
					DestWriter.Write(UncompressedSize);
					DestWriter.Write(FilenameLen);
					DestWriter.Write((ushort)(ExtraLen + ExtraAlign));
					DestWriter.BaseStream.Write(Filename, 0, FilenameLen);
					DestWriter.BaseStream.Write(Extra, 0, ExtraLen);
					while (ExtraAlign-- > 0)
					{
						DestWriter.Write((byte)0);
					}

					// copy file data as-is
					Remaining = CompressedSize;
					while (Remaining > 0)
					{
						int CopySize = Remaining < 65536 ? (int)Remaining : 65536;
						int BytesRead = SourceReader.BaseStream.Read(CopyBuffer, 0, CopySize);
						DestWriter.BaseStream.Write(CopyBuffer, 0, BytesRead);
						Remaining -= (uint)BytesRead;
					}

					// write central directory entry for this file
					DirectoryWriter.Write(Signature);
					DirectoryWriter.Write(DirVersion);
					DirectoryWriter.Write(Version);
					DirectoryWriter.Write(Flags);
					DirectoryWriter.Write(Compression);
					DirectoryWriter.Write(ModTime);
					DirectoryWriter.Write(ModDate);
					DirectoryWriter.Write(CRC32Value);
					DirectoryWriter.Write(CompressedSize);
					DirectoryWriter.Write(UncompressedSize);
					DirectoryWriter.Write(FilenameLen);
					DirectoryWriter.Write(ExtraLen);
					DirectoryWriter.Write(CommentLen);
					DirectoryWriter.Write(DiskStart);
					DirectoryWriter.Write(InternalAttr);
					DirectoryWriter.Write(ExternalAttr);
					DirectoryWriter.Write(Location);
					DirectoryWriter.BaseStream.Write(Filename, 0, FilenameLen);
					DirectoryWriter.BaseStream.Write(Extra, 0, ExtraLen);
					// write comment here if there was one
				}
				else
				{
					// replace with the new .SO
					try
					{
						FileStream SourceFile = File.OpenRead(SourceSOFile);

						UncompressedSize = (uint)SourceFile.Length;
						CompressedSize = 0;
						Compression = 8;        // deflate

						// calculate CRC32
						CRC32Value = 0x0;
						CRC32Value = ~CRC32Value;
						Remaining = UncompressedSize;
						while (Remaining > 0)
						{
							int CopySize = Remaining < 65536 ? (int)Remaining : 65536;
							int BytesRead = SourceFile.Read(CopyBuffer, 0, CopySize);
							Remaining -= (uint)BytesRead;
							for (uint Index = 0; Index < BytesRead; Index++)
							{
								CRC32Value = (CRC32Value >> 8) ^ CRCTablesSB8[(CRC32Value ^ (uint)CopyBuffer[Index]) & 0xff];
							}
						}
						CRC32Value = ~CRC32Value;
						SourceFile.Position = 0;

						TotalEntries++;
						uint Location = (uint)DestWriter.BaseStream.Position;

						using (MemoryStream CompressedStream = new MemoryStream())
						{
							using (DeflateStream Compressor = new DeflateStream(CompressedStream, CompressionMode.Compress, leaveOpen: true))
							{
								SourceFile.CopyTo(Compressor);
								SourceFile.Close();
							}

							byte[] CompressedBuffer = CompressedStream.GetBuffer();
							CompressedSize = (uint)CompressedStream.Length;

							// calculate location after header and any additional alignment needed (only if uncompressed)
							uint HeaderSize = (uint)(4 + 2 + 2 + 2 + 2 + 2 + 4 + 4 + 4 + 2 + 2 + FilenameLen + ExtraLen);
							uint HeaderEnd = Location + HeaderSize;
							uint DataStart = (uint)((HeaderEnd + 3) & ~3);
							ushort ExtraAlign = (ushort)(Compression != 0 ? 0 : (DataStart - HeaderEnd));

							// write local header
							DestWriter.Write(Header);
							DestWriter.Write(Version);
							DestWriter.Write(Flags);
							DestWriter.Write(Compression);
							DestWriter.Write(ModTime);
							DestWriter.Write(ModDate);
							DestWriter.Write(CRC32Value);
							DestWriter.Write(CompressedSize);
							DestWriter.Write(UncompressedSize);
							DestWriter.Write(FilenameLen);
							DestWriter.Write((ushort)(ExtraLen + ExtraAlign));
							DestWriter.BaseStream.Write(Filename, 0, FilenameLen);
							DestWriter.BaseStream.Write(Extra, 0, ExtraLen);
							while (ExtraAlign-- > 0)
							{
								DestWriter.Write((byte)0);
							}

							// write compressed data
							DestWriter.BaseStream.Write(CompressedBuffer, 0, (int)CompressedSize);
						}

						// write central directory entry for this file
						DirectoryWriter.Write(Signature);
						DirectoryWriter.Write(DirVersion);
						DirectoryWriter.Write(Version);
						DirectoryWriter.Write(Flags);
						DirectoryWriter.Write(Compression);
						DirectoryWriter.Write(ModTime);
						DirectoryWriter.Write(ModDate);
						DirectoryWriter.Write(CRC32Value);
						DirectoryWriter.Write(CompressedSize);
						DirectoryWriter.Write(UncompressedSize);
						DirectoryWriter.Write(FilenameLen);
						DirectoryWriter.Write(ExtraLen);
						DirectoryWriter.Write(CommentLen);
						DirectoryWriter.Write(DiskStart);
						DirectoryWriter.Write(InternalAttr);
						DirectoryWriter.Write(ExternalAttr);
						DirectoryWriter.Write(Location);
						DirectoryWriter.BaseStream.Write(Filename, 0, FilenameLen);
						DirectoryWriter.BaseStream.Write(Extra, 0, ExtraLen);
						// write comment here if there was one
					}
					catch (IOException e)
					{
						Logger.LogInformation("Failed to add {SourceSOFile} to APK: Reason = {Reason}", SourceSOFile, e.ToString());
						DirectoryWriter.Close();
						File.Delete(DirectoryFilename);
						DestWriter.Close();
						File.Delete(DestAPK);
						return false;
					}
				}
			}

			uint CentralDirectoryLocation = (uint)DestWriter.BaseStream.Position;

			// copy the directory to the end of the APK
			uint DirectorySize = (uint)DirectoryWriter.BaseStream.Position;
			DirectoryWriter.BaseStream.Position = 0;
			Remaining = DirectorySize;
			while (Remaining > 0)
			{
				int CopySize = Remaining < 65536 ? (int)Remaining : 65536;
				int BytesRead = DirectoryWriter.BaseStream.Read(CopyBuffer, 0, CopySize);
				DestWriter.BaseStream.Write(CopyBuffer, 0, BytesRead);
				Remaining -= (uint)BytesRead;
			}
			DirectoryWriter.Close();
			File.Delete(DirectoryFilename);

			// write end of central directory record
			Signature = 0x06054b50;
			DestWriter.Write(Signature);
			DestWriter.Write(DiskNumber);
			DestWriter.Write(DiskStart);
			DestWriter.Write(TotalEntries);
			DestWriter.Write(TotalEntries);
			DestWriter.Write(DirectorySize);
			DestWriter.Write(CentralDirectoryLocation);
			DestWriter.Write(CommentLen);
			DestWriter.Close();
			return true;
		}

		private void CreateGradlePropertiesFiles(AndroidToolChain ToolChain, UnrealArch Arch, int MinSDKVersion, int TargetSDKVersion, string CompileSDKVersion, string BuildToolsVersion, string PackageName,
			string? DestApkName, string NDKArch, string UnrealBuildFilesPath, string GameBuildFilesPath, string UnrealBuildGradleAppPath, string UnrealBuildPath, string UnrealBuildGradlePath,
			bool bForDistribution, bool bIsEmbedded, List<string> OBBFiles)
		{
			// Create gradle.properties
			StringBuilder GradleProperties = new StringBuilder();

			int StoreVersion = GetStoreVersion(GetUnrealArch(NDKArch));
			string VersionDisplayName = GetVersionDisplayName(bIsEmbedded);

			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);

			bool bEnableUniversalAPK = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableUniversalAPK", out bEnableUniversalAPK);

			GradleProperties.AppendLine("org.gradle.daemon=false");
			GradleProperties.AppendLine("org.gradle.jvmargs=-XX:MaxHeapSize=4096m -Xmx9216m");
			GradleProperties.AppendLine("android.injected.testOnly=false");
			GradleProperties.AppendLine("android.useAndroidX=true");
			GradleProperties.AppendLine("android.enableJetifier=true");
			GradleProperties.AppendLine(String.Format("COMPILE_SDK_VERSION={0}", CompileSDKVersion));
			GradleProperties.AppendLine(String.Format("BUILD_TOOLS_VERSION={0}", BuildToolsVersion));
			GradleProperties.AppendLine(String.Format("PACKAGE_NAME={0}", PackageName));
			GradleProperties.AppendLine(String.Format("MIN_SDK_VERSION={0}", MinSDKVersion.ToString()));
			GradleProperties.AppendLine(String.Format("TARGET_SDK_VERSION={0}", TargetSDKVersion.ToString()));
			GradleProperties.AppendLine(String.Format("STORE_VERSION={0}", StoreVersion.ToString()));
			GradleProperties.AppendLine(String.Format("VERSION_DISPLAY_NAME={0}", VersionDisplayName));

			string NDKPath = Environment.GetEnvironmentVariable("NDKROOT")!.Replace("\\", "/");
			int NDKVersionIndex = NDKPath.LastIndexOf("/");
			string NDKVersion = NDKVersionIndex > 0 ? NDKPath.Substring(NDKVersionIndex + 1) : DEFAULT_NDK_VERSION;
			GradleProperties.AppendLine(String.Format("NDK_VERSION={0}", NDKVersion));

			if (DestApkName != null)
			{
				GradleProperties.AppendLine(String.Format("OUTPUT_PATH={0}", Path.GetDirectoryName(DestApkName)!.Replace("\\", "/")));
				GradleProperties.AppendLine(String.Format("OUTPUT_FILENAME={0}", Path.GetFileName(DestApkName)));

				string BundleFilename = Path.GetFileName(DestApkName).Replace(".apk", ".aab");
				GradleProperties.AppendLine(String.Format("OUTPUT_BUNDLEFILENAME={0}", BundleFilename));

				if (bEnableUniversalAPK)
				{
					string UniversalAPKFilename = Path.GetFileName(DestApkName).Replace(".apk", "_universal.apk");
					GradleProperties.AppendLine("OUTPUT_UNIVERSALFILENAME=" + UniversalAPKFilename);
				}
			}

			int OBBFileIndex = 0;
			GradleProperties.AppendLine(String.Format("OBB_FILECOUNT={0}", OBBFiles.Count));
			foreach (string OBBFile in OBBFiles)
			{
				GradleProperties.AppendLine(String.Format("OBB_FILE{0}={1}", OBBFileIndex++, OBBFile.Replace("\\", "/")));
			}

			GradleProperties.AppendLine("ANDROID_TOOLS_BUILD_GRADLE_VERSION={0}", ANDROID_TOOLS_BUILD_GRADLE_VERSION);
			GradleProperties.AppendLine("BUNDLETOOL_JAR=" + Path.GetFullPath(Path.Combine(UnrealBuildFilesPath, "..", "Prebuilt", "bundletool", BUNDLETOOL_JAR)).Replace("\\", "/"));
			GradleProperties.AppendLine("GENUNIVERSALAPK_JAR=" + Path.GetFullPath(Path.Combine(UnrealBuildFilesPath, "..", "Prebuilt", "GenUniversalAPK", "bin", "GenUniversalAPK.jar")).Replace("\\", "/"));

			// add any Gradle properties from UPL
			string GradlePropertiesUPL = UPL!.ProcessPluginNode(NDKArch, "gradleProperties", "");
			GradleProperties.AppendLine(GradlePropertiesUPL);

			// Create abi.gradle
			StringBuilder ABIGradle = new StringBuilder();
			ABIGradle.AppendLine("android {");
			ABIGradle.AppendLine("\tdefaultConfig {");
			ABIGradle.AppendLine("\t\tndk {");
			ABIGradle.AppendLine(String.Format("\t\t\tabiFilter \"{0}\"", NDKArch));
			ABIGradle.AppendLine("\t\t}");
			ABIGradle.AppendLine("\t}");
			ABIGradle.AppendLine("}");
			string ABIGradleFilename = Path.Combine(UnrealBuildGradleAppPath, "abi.gradle");
			File.WriteAllText(ABIGradleFilename, ABIGradle.ToString());

			StringBuilder GradleBuildAdditionsContent = new StringBuilder();
			GradleBuildAdditionsContent.AppendLine("apply from: 'aar-imports.gradle'");
			GradleBuildAdditionsContent.AppendLine("apply from: 'projects.gradle'");
			GradleBuildAdditionsContent.AppendLine("apply from: 'abi.gradle'");

			bool bEnableBundle, bBundleABISplit, bBundleLanguageSplit, bBundleDensitySplit;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableBundle", out bEnableBundle);
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBundleABISplit", out bBundleABISplit);
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBundleLanguageSplit", out bBundleLanguageSplit);
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBundleDensitySplit", out bBundleDensitySplit);

			GradleBuildAdditionsContent.AppendLine("android {");

			bool bExtractNativeLibs = true;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bExtractNativeLibs", out bExtractNativeLibs);
			AndroidToolChain.ClangSanitizer Sanitizer = ToolChain.BuildWithSanitizer();
			// hwasan on NDK r26b+ requires wrap.sh that needs to be unpacked
			if ((Sanitizer != AndroidToolChain.ClangSanitizer.None && (Sanitizer != AndroidToolChain.ClangSanitizer.HwAddress || ToolChain.HasEmbeddedHWASanSupport())) || bEnableScudoMemoryTracing)
			{
				bExtractNativeLibs = true;
			}
			if (bExtractNativeLibs)
			{
				GradleBuildAdditionsContent.AppendLine("\tpackagingOptions {");
				GradleBuildAdditionsContent.AppendLine("\t\tjniLibs {");
				GradleBuildAdditionsContent.AppendLine("\t\t\tuseLegacyPackaging=true");
				GradleBuildAdditionsContent.AppendLine("\t\t}");
				GradleBuildAdditionsContent.AppendLine("\t}");
			}

			if (!ForceAPKGeneration && bEnableBundle)
			{
				GradleBuildAdditionsContent.AppendLine("\tbundle {");
				GradleBuildAdditionsContent.AppendLine("\t\tabi { enableSplit = " + (bBundleABISplit ? "true" : "false") + " }");
				GradleBuildAdditionsContent.AppendLine("\t\tlanguage { enableSplit = " + (bBundleLanguageSplit ? "true" : "false") + " }");
				GradleBuildAdditionsContent.AppendLine("\t\tdensity { enableSplit = " + (bBundleDensitySplit ? "true" : "false") + " }");
				GradleBuildAdditionsContent.AppendLine("\t}");
			}

			if (bForDistribution)
			{
				string KeyAlias, KeyStore, KeyStorePassword, KeyPassword;
				Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStore", out KeyStore);
				Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyAlias", out KeyAlias);
				Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStorePassword", out KeyStorePassword);
				Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyPassword", out KeyPassword);

				if (String.IsNullOrEmpty(KeyStore) || String.IsNullOrEmpty(KeyAlias) || String.IsNullOrEmpty(KeyStorePassword))
				{
					throw new BuildException("DistributionSigning settings are not all set. Check the DistributionSettings section in the Android tab of Project Settings");
				}

				if (String.IsNullOrEmpty(KeyPassword) || KeyPassword == "_sameaskeystore_")
				{
					KeyPassword = KeyStorePassword;
				}

				// Make sure the keystore file exists
				string KeyStoreFilename = Path.Combine(UnrealBuildPath, KeyStore);
				if (!File.Exists(KeyStoreFilename))
				{
					throw new BuildException("Keystore file is missing. Check the DistributionSettings section in the Android tab of Project Settings");
				}

				GradleProperties.AppendLine(String.Format("STORE_FILE={0}", KeyStoreFilename.Replace("\\", "/")));
				GradleProperties.AppendLine(String.Format("STORE_PASSWORD={0}", KeyStorePassword));
				GradleProperties.AppendLine(String.Format("KEY_ALIAS={0}", KeyAlias));
				GradleProperties.AppendLine(String.Format("KEY_PASSWORD={0}", KeyPassword));

				GradleBuildAdditionsContent.AppendLine("\tsigningConfigs {");
				GradleBuildAdditionsContent.AppendLine("\t\trelease {");
				GradleBuildAdditionsContent.AppendLine(String.Format("\t\t\tstoreFile file('{0}')", KeyStoreFilename.Replace("\\", "/")));
				GradleBuildAdditionsContent.AppendLine(String.Format("\t\t\tstorePassword '{0}'", KeyStorePassword));
				GradleBuildAdditionsContent.AppendLine(String.Format("\t\t\tkeyAlias '{0}'", KeyAlias));
				GradleBuildAdditionsContent.AppendLine(String.Format("\t\t\tkeyPassword '{0}'", KeyPassword));
				GradleBuildAdditionsContent.AppendLine("\t\t}");
				GradleBuildAdditionsContent.AppendLine("\t}");

				// Generate the Proguard file contents and write it
				string ProguardContents = GenerateProguard(NDKArch, UnrealBuildFilesPath, GameBuildFilesPath);
				string ProguardFilename = Path.Combine(UnrealBuildGradleAppPath, "proguard-rules.pro");
				SafeDeleteFile(ProguardFilename);
				File.WriteAllText(ProguardFilename, ProguardContents);
			}
			else
			{
				// empty just for Gradle not to complain
				GradleProperties.AppendLine("STORE_FILE=");
				GradleProperties.AppendLine("STORE_PASSWORD=");
				GradleProperties.AppendLine("KEY_ALIAS=");
				GradleProperties.AppendLine("KEY_PASSWORD=");

				// empty just for Gradle not to complain
				GradleBuildAdditionsContent.AppendLine("\tsigningConfigs {");
				GradleBuildAdditionsContent.AppendLine("\t\trelease {");
				GradleBuildAdditionsContent.AppendLine("\t\t}");
				GradleBuildAdditionsContent.AppendLine("\t}");
			}

			GradleBuildAdditionsContent.AppendLine("\tbuildTypes {");
			GradleBuildAdditionsContent.AppendLine("\t\trelease {");
			GradleBuildAdditionsContent.AppendLine("\t\t\tsigningConfig signingConfigs.release");
			if (GradlePropertiesUPL.Contains("DISABLE_MINIFY=1"))
			{
				GradleBuildAdditionsContent.AppendLine("\t\t\tminifyEnabled false");
			}
			else
			{
				GradleBuildAdditionsContent.AppendLine("\t\t\tminifyEnabled true");
			}
			if (GradlePropertiesUPL.Contains("DISABLE_PROGUARD=1"))
			{
				GradleBuildAdditionsContent.AppendLine("\t\t\tuseProguard false");
			}
			else
			{
				GradleBuildAdditionsContent.AppendLine("\t\t\tproguardFiles getDefaultProguardFile('proguard-android.txt'), 'proguard-rules.pro'");
			}
			GradleBuildAdditionsContent.AppendLine("\t\t}");
			GradleBuildAdditionsContent.AppendLine("\t\tdebug {");
			GradleBuildAdditionsContent.AppendLine("\t\t\tdebuggable true");
			GradleBuildAdditionsContent.AppendLine("\t\t}");
			GradleBuildAdditionsContent.AppendLine("\t}");
			GradleBuildAdditionsContent.AppendLine("}");

			// Add any UPL app buildGradleAdditions
			GradleBuildAdditionsContent.Append(UPL.ProcessPluginNode(NDKArch, "buildGradleAdditions", ""));

			string GradleBuildAdditionsFilename = Path.Combine(UnrealBuildGradleAppPath, "buildAdditions.gradle");
			File.WriteAllText(GradleBuildAdditionsFilename, GradleBuildAdditionsContent.ToString());

			string GradlePropertiesFilename = Path.Combine(UnrealBuildGradlePath, "gradle.properties");
			File.WriteAllText(GradlePropertiesFilename, GradleProperties.ToString());

			// Add lint if requested (note depreciation warnings can be suppressed with @SuppressWarnings("deprecation")
			string GradleBaseBuildAdditionsContents = "";
			bool bEnableLint = false;
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableLint", out bEnableLint);
			if (bEnableLint)
			{
				GradleBaseBuildAdditionsContents =
					"allprojects {\n" +
					"\ttasks.withType(JavaCompile) {\n" +
					"\t\toptions.compilerArgs << \"-Xlint:unchecked\" << \"-Xlint:deprecation\"\n" +
					"\t}\n" +
					"}\n\n";
			}

			// Create baseBuildAdditions.gradle from plugins baseBuildGradleAdditions
			string GradleBaseBuildAdditionsFilename = Path.Combine(UnrealBuildGradlePath, "baseBuildAdditions.gradle");
			File.WriteAllText(GradleBaseBuildAdditionsFilename, UPL.ProcessPluginNode(NDKArch, "baseBuildGradleAdditions", GradleBaseBuildAdditionsContents));

			// Create buildscriptAdditions.gradle from plugins buildscriptGradleAdditions
			string GradleBuildScriptAdditionsFilename = Path.Combine(UnrealBuildGradlePath, "buildscriptAdditions.gradle");
			File.WriteAllText(GradleBuildScriptAdditionsFilename, UPL.ProcessPluginNode(NDKArch, "buildscriptGradleAdditions", ""));
		}

		public static bool GetDontBundleLibrariesInAPK(FileReference? ProjectFile, bool? bForceDontBundleLibrariesInAPK, UnrealTargetConfiguration Configuration, bool bIsArchive, bool bFromMSBuild, bool bIsFromUAT, ILogger? Logger)
		{
			if (bForceDontBundleLibrariesInAPK.HasValue)
			{
				Logger?.LogInformation("bDontBundleLibrariesInAPK is force set to {bForceDontBundleLibrariesInAPK} via command line option.", bForceDontBundleLibrariesInAPK.Value);
				return bForceDontBundleLibrariesInAPK.Value;
			}

			// this feature requires extra step to install the app besides just .apk install
			// limit where the feature is enabled because we're not in control of deployment step but in VS/AGDE and UAT
			if (!(bFromMSBuild || bIsFromUAT)) 
			{
				Logger?.LogInformation("bDontBundleLibrariesInAPK is set to false, due to not called from MSBuild or UAT (bFromMSBuild={bFromMSBuild}, bIsFromUAT={bIsFromUAT}), use \"-ForceDontBundleLibrariesInAPK=true\" to override.", bFromMSBuild, bIsFromUAT);
				return false;
			}

			if (Configuration == UnrealTargetConfiguration.Shipping)
			{
				Logger?.LogInformation("bDontBundleLibrariesInAPK is set to false, due to a shipping build.");
				return false;
			}

			if (bIsArchive)
			{
				Logger?.LogInformation("bDontBundleLibrariesInAPK is set to false, due to an archive build.");
				return false;
			}

			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bDontBundleLibrariesInAPK", out bool bDontBundleLibrariesInAPK);
			Logger?.LogInformation("bDontBundleLibrariesInAPK is set to {bDontBundleLibrariesInAPK}, based on AndroidRuntimeSettings.", bDontBundleLibrariesInAPK);
			return bDontBundleLibrariesInAPK;
		}

		private bool GetDontBundleLibrariesInAPK(UnrealTargetConfiguration Configuration, bool bIsArchive, bool bIsFromUAT, bool bVerbose = false)
		{
			return GetDontBundleLibrariesInAPK(ProjectFile, ForceDontBundleLibrariesInAPK, Configuration, bIsArchive, bFromMSBuild, bIsFromUAT, bVerbose ? Logger : null);
		}

		// Architecture remapping
		private static readonly Dictionary<string, string> ArchRemapping = new()
		{
			{"arm64-v8a", "arm64"},
			{"x86_64", "x64"}
		};

		private void MakeApk(AndroidToolChain ToolChain, string ProjectName, TargetType InTargetType, string ProjectDirectory, string OutputPath, string EngineDirectory, bool bForDistribution, string CookFlavor,
			UnrealTargetConfiguration Configuration, bool bMakeSeparateApks, bool bIncrementalPackage, bool bDisallowPackagingDataInApk, bool bDisallowExternalFilesDir, bool bSkipGradleBuild, bool bIsArchive, bool bIsFromUAT)
		{
			Logger.LogInformation("");
			Logger.LogInformation("===={Time}====PREPARING TO MAKE APK=================================================================", DateTime.Now.ToString());

			if (Architectures == null)
			{
				throw new BuildException("Called MakeApk without first calling SetAndroidPluginData");
			}

			// we do not need to really build an engine UnrealGame.apk so short-circuit it
			if (!ForceAPKGeneration && ProjectName == "UnrealGame" && OutputPath.Replace("\\", "/").Contains("/Engine/Binaries/Android/") && Path.GetFileNameWithoutExtension(OutputPath).StartsWith("UnrealGame"))
			{
				if (!bSkipGradleBuild)
				{
					/*
					IEnumerable<Tuple<string, string>> TargetList = null;

					TargetList = from Arch in Arches
								from GPUArch in GPUArchitectures
								select Tuple.Create(Arch, GPUArch);

					string DestApkDirectory = Path.Combine(ProjectDirectory, "Binaries/Android");
					string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UnrealGame", ProjectName);

					foreach (Tuple<string, string> target in TargetList)
					{
						string Arch = target.Item1;
						string GPUArchitecture = target.Item2;
						string DestApkName = Path.Combine(DestApkDirectory, ApkFilename + ".apk");
						DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch, GPUArchitecture);

						// create a dummy APK if doesn't exist
						if (!File.Exists(DestApkName))
						{
							File.WriteAllText(DestApkName, "dummyfile");
						}
					}
					*/
				}
				Logger.LogInformation("APK generation not needed for project {ProjectName} with {OutputPath}", ProjectName, OutputPath);
				Logger.LogInformation("");
				Logger.LogInformation("===={Time}====COMPLETED MAKE APK=======================================================================", DateTime.Now.ToString());
				return;
			}

			if (UPL!.GetLastError() != null)
			{
				throw new BuildException("Cannot make APK with UPL errors");
			}

			// make sure it is cached (clear unused warning)
			string EngineVersion = ReadEngineVersion();
			if (EngineVersion == null)
			{
				throw new BuildException("No engine version!");
			}

			SetMinimumSDKLevelForGradle();

			// Verify license agreement since we require Gradle
			if (!IsLicenseAgreementValid())
			{
				throw new BuildException("Android SDK license file not found.  Please agree to license in Android project settings in the editor.");
			}

			LogBuildSetup();

			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);

			// bundles disabled for launch-on
			bool bEnableBundle = BundleEnabled() && !bDisallowPackagingDataInApk;

			bool bIsBuildMachine = Unreal.IsBuildMachine();

			// do this here so we'll stop early if there is a problem with the SDK API level (cached so later calls will return the same)
			string SDKAPILevel = GetSdkApiLevel(ToolChain);
			int SDKLevelInt = GetApiLevelInt(SDKAPILevel);
			string BuildToolsVersion = GetBuildToolsVersion();

			// cache some tools paths
			//string NDKBuildPath = Environment.ExpandEnvironmentVariables("%NDKROOT%/ndk-build" + (RuntimePlatform.IsWindows ? ".cmd" : ""));
			//bool HasNDKPath = File.Exists(NDKBuildPath);

			// set up some directory info
			string IntermediateAndroidPath = Path.Combine(ProjectDirectory, "Intermediate", "Android");
			string UnrealJavaFilePath = Path.Combine(ProjectDirectory, "Build", "Android", GetUnrealJavaSrcPath());
			string UnrealBuildFilesPath = GetUnrealBuildFilePath(EngineDirectory);
			string UnrealPreBuiltFilesPath = GetUnrealPreBuiltFilePath(EngineDirectory);
			string UnrealBuildFilesPath_NFL = GetUnrealBuildFilePath(Path.Combine(EngineDirectory, "Restricted", "NotForLicensees"));
			string UnrealBuildFilesPath_NR = GetUnrealBuildFilePath(Path.Combine(EngineDirectory, "Restricted", "NoRedist"));
			string GameBuildFilesPath = Path.Combine(ProjectDirectory, "Build", "Android");
			string GameBuildFilesPath_NFL = Path.Combine(Path.Combine(ProjectDirectory, "Restricted","NotForLicensees"), "Build", "Android");
			string GameBuildFilesPath_NR = Path.Combine(Path.Combine(ProjectDirectory, "Restricted", "NoRedist"), "Build", "Android");

			// get a list of unique NDK architectures enabled for build
			List<string> NDKArches = new List<string>();
			foreach (UnrealArch Arch in Architectures.Architectures)
			{
				string NDKArch = GetNDKArch(Arch);
				if (!NDKArches.Contains(NDKArch))
				{
					NDKArches.Add(NDKArch);
				}
			}

			// force create from scratch if on build machine
			bool bCreateFromScratch = bIsBuildMachine;

			AndroidToolChain.ClangSanitizer Sanitizer = ToolChain.BuildWithSanitizer();

			// see if last time matches the skipGradle setting
			string BuildTypeFilename = Path.Combine(IntermediateAndroidPath, "BuildType.txt");
			string BuildTypeID = bSkipGradleBuild ? "Embedded" : "Standalone";
			
			// hwasan on NDK r26b+ requires wrap.sh that needs to be unpacked
			if (Sanitizer != AndroidToolChain.ClangSanitizer.None && (Sanitizer != AndroidToolChain.ClangSanitizer.HwAddress || ToolChain.HasEmbeddedHWASanSupport()))
			{
				BuildTypeID += Sanitizer.ToString() + "Sanitizer";
			}
			else if (bEnableScudoMemoryTracing)
			{
				BuildTypeID += Sanitizer.ToString() + "ScudoMemoryTrace";
			}
			if (File.Exists(BuildTypeFilename))
			{
				string BuildTypeContents = File.ReadAllText(BuildTypeFilename);
				if (BuildTypeID != BuildTypeContents)
				{
					Logger.LogInformation("Build type changed, forcing clean");
					bCreateFromScratch = true;
				}
			}

			// force cleanup if older UE4 project
			if (File.Exists(Path.Combine(IntermediateAndroidPath, "arm64", "jni", "arm64-v8a", "libUE4.so")) ||
				File.Exists(Path.Combine(IntermediateAndroidPath, "x64", "jni", "x86_64", "libUE4.so")))
			{
				Logger.LogInformation("Old version of library .so found, forcing clean");
				bCreateFromScratch = true;
			}

			// check if the enabled plugins has changed
			string PluginListFilename = Path.Combine(IntermediateAndroidPath, "ActiveUPL.txt");
			string PluginListContents = ActiveUPLFiles.ToString();
			if (File.Exists(PluginListFilename))
			{
				string PreviousPluginListContents = File.ReadAllText(PluginListFilename);
				if (PluginListContents != PreviousPluginListContents)
				{
					Logger.LogInformation("Active UPL files changed, forcing clean");
					bCreateFromScratch = true;
				}
			}

			if (bCreateFromScratch)
			{
				Logger.LogInformation("Cleaning {IntermediateAndroidPath}", IntermediateAndroidPath);
				DeleteDirectory(IntermediateAndroidPath, Logger);
				Directory.CreateDirectory(IntermediateAndroidPath);
			}

			if (!System.IO.Directory.Exists(IntermediateAndroidPath))
			{
				System.IO.Directory.CreateDirectory(IntermediateAndroidPath);
			}

			// write enabled plugins list
			File.WriteAllText(PluginListFilename, PluginListContents);

			// write build type
			File.WriteAllText(BuildTypeFilename, BuildTypeID);

			// cache if we want data in the Apk
			bool bPackageDataInsideApk = bDisallowPackagingDataInApk ? false : GetPackageDataInsideApk();
			bool bDisableVerifyOBBOnStartUp = DisableVerifyOBBOnStartUp();
			bool bUseExternalFilesDir = UseExternalFilesDir(bDisallowExternalFilesDir);

			// Generate Java files
			string PackageName = GetPackageName(ProjectName);
			string TemplateDestinationBase = Path.Combine(ProjectDirectory, "Build", "Android", "src", PackageName.Replace('.', Path.DirectorySeparatorChar));
			MakeDirectoryIfRequired(TemplateDestinationBase);

			// We'll be writing the OBB data into the same location as the download service files
			string UnrealOBBDataFileName = GetUnrealJavaOBBDataFileName(TemplateDestinationBase);
			string UnrealDownloadShimFileName = GetUnrealJavaDownloadShimFileName(UnrealJavaFilePath);

			// Template generated files
			string JavaTemplateSourceDir = GetUnrealTemplateJavaSourceDir(EngineDirectory);
			IEnumerable<TemplateFile> templates = from template in Directory.EnumerateFiles(JavaTemplateSourceDir, "*.template")
												  let RealName = Path.GetFileNameWithoutExtension(template)
												  select new TemplateFile(SourceFile: template, DestinationFile: GetUnrealTemplateJavaDestination(TemplateDestinationBase, RealName));

			// Generate the OBB and Shim files here
			string ObbFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + ".obb";
			string PatchFileLocation = ProjectDirectory + "/Saved/StagedBuilds/Android" + CookFlavor + ".patch.obb";
			List<string> RequiredOBBFiles = new List<String> { ObbFileLocation };
			if (File.Exists(PatchFileLocation))
			{
				RequiredOBBFiles.Add(PatchFileLocation);
			}

			// Generate the OBBData.java file if out of date (can skip rewriting it if packaging inside Apk in some cases)
			// Note: this may be replaced per architecture later if store version is different
			WriteJavaOBBDataFile(UnrealOBBDataFileName, PackageName, RequiredOBBFiles, CookFlavor, bPackageDataInsideApk, Architectures.Architectures[0]);

			// Make sure any existing proguard file in project is NOT used (back it up)
			string ProjectBuildProguardFile = Path.Combine(GameBuildFilesPath, "proguard-project.txt");
			if (File.Exists(ProjectBuildProguardFile))
			{
				string ProjectBackupProguardFile = Path.Combine(GameBuildFilesPath, "proguard-project.backup");
				File.Move(ProjectBuildProguardFile, ProjectBackupProguardFile);
			}

			WriteJavaDownloadSupportFiles(UnrealDownloadShimFileName, templates, new Dictionary<string, string>{
				{ "$$GameName$$", ProjectName },
				{ "$$PublicKey$$", GetPublicKey() },
				{ "$$PackageName$$",PackageName }
			});

			// Sometimes old files get left behind if things change, so we'll do a clean up pass
			foreach (string NDKArch in NDKArches)
			{
				string UnrealBuildPath = Path.Combine(IntermediateAndroidPath, GetUnrealArch(NDKArch).ToString());

				string CleanUpBaseDir = Path.Combine(ProjectDirectory, "Build", "Android", "src");
				string ImmediateBaseDir = Path.Combine(UnrealBuildPath, "src");
				IEnumerable<string> files = Directory.EnumerateFiles(CleanUpBaseDir, "*.java", SearchOption.AllDirectories);

				Logger.LogInformation("Cleaning up files based on template dir {TemplateDestinationBase}", TemplateDestinationBase);

				// Make a set of files that are okay to clean up
				HashSet<string> cleanFiles = new HashSet<string>();
				cleanFiles.Add("DownloadShim.java");
				cleanFiles.Add("OBBData.java");
				foreach (TemplateFile template in templates)
				{
					cleanFiles.Add(Path.GetFileName(template.DestinationFile));
				}

				foreach (string filename in files)
				{
					// keep the shim if it is in the right place
					if (filename == UnrealDownloadShimFileName)
					{
						continue;
					}

					string filePath = Path.GetDirectoryName(filename)!;  // grab the file's path
					if (filePath != TemplateDestinationBase)             // and check to make sure it isn't the same as the Template directory we calculated earlier
					{
						// Only delete the files in the cleanup set
						if (!cleanFiles.Contains(Path.GetFileName(filename)))
						{
							continue;
						}

						Logger.LogInformation("Cleaning up file {File}", filename);
						SafeDeleteFile(filename, false);

						// Check to see if this file also exists in our target destination, and if so delete it too
						string DestFilename = Path.Combine(ImmediateBaseDir, Utils.MakePathRelativeTo(filename, CleanUpBaseDir));
						if (File.Exists(DestFilename))
						{
							Logger.LogInformation("Cleaning up file {DestFilename}", DestFilename);
							SafeDeleteFile(DestFilename, false);
						}
					}
				}

				// Directory clean up code (Build/Android/src)
				try
				{
					IEnumerable<string> BaseDirectories = Directory.EnumerateDirectories(CleanUpBaseDir, "*", SearchOption.AllDirectories).OrderByDescending(x => x);
					foreach (string directory in BaseDirectories)
					{
						if (Directory.Exists(directory) && Directory.GetFiles(directory, "*.*", SearchOption.AllDirectories).Count() == 0)
						{
							Logger.LogInformation("Cleaning Directory {Directory} as empty.", directory);
							Directory.Delete(directory, true);
						}
					}
				}
				catch (Exception)
				{
					// likely System.IO.DirectoryNotFoundException, ignore it
				}

				// Directory clean up code (Intermediate/APK/src)
				try
				{
					IEnumerable<string> ImmediateDirectories = Directory.EnumerateDirectories(ImmediateBaseDir, "*", SearchOption.AllDirectories).OrderByDescending(x => x);
					foreach (string directory in ImmediateDirectories)
					{
						if (Directory.Exists(directory) && Directory.GetFiles(directory, "*.*", SearchOption.AllDirectories).Count() == 0)
						{
							Logger.LogInformation("Cleaning Directory {Directory} as empty.", directory);
							Directory.Delete(directory, true);
						}
					}
				}
				catch (Exception)
				{
					// likely System.IO.DirectoryNotFoundException, ignore it
				}
			}

			bool bDontBundleLibrariesInAPK = GetDontBundleLibrariesInAPK(Configuration, bIsArchive, bIsFromUAT, bVerbose: !bIsFromUAT);

			// check to see if any "meta information" is newer than last time we build
			string TemplatesHashCode = GenerateTemplatesHashCode(EngineDirectory);
			string CurrentBuildSettings = GetAllBuildSettings(ToolChain, UPL!, bForDistribution, bMakeSeparateApks, bPackageDataInsideApk, bDisableVerifyOBBOnStartUp, bUseExternalFilesDir, bDontBundleLibrariesInAPK, TemplatesHashCode);
			string BuildSettingsCacheFile = Path.Combine(IntermediateAndroidPath, "UEBuildSettings.txt");

			// do we match previous build settings?
			bool bBuildSettingsMatch = true;

			// get application name and whether it changed, needing to force repackage
			string? ApplicationDisplayName;
			if (CheckApplicationName(Path.Combine(IntermediateAndroidPath, ArchRemapping[NDKArches[0]]), ProjectName, out ApplicationDisplayName))
			{
				bBuildSettingsMatch = false;
				Logger.LogInformation("Application display name is different than last build, forcing repackage.");
			}

			// if the manifest matches, look at other settings stored in a file
			if (bBuildSettingsMatch)
			{
				if (File.Exists(BuildSettingsCacheFile))
				{
					string PreviousBuildSettings = File.ReadAllText(BuildSettingsCacheFile);
					if (PreviousBuildSettings != CurrentBuildSettings)
					{
						bBuildSettingsMatch = false;
						Logger.LogInformation("Previous .apk file(s) were made with different build settings, forcing repackage.");
					}
				}
			}

			// only check input dependencies if the build settings already match (if we don't run gradle, there is no Apk file to check against)
			if (bBuildSettingsMatch && !bSkipGradleBuild)
			{
				// check if so's are up to date against various inputs
				List<string> JavaFiles = new List<string>{
													UnrealOBBDataFileName,
													UnrealDownloadShimFileName
												};
				// Add the generated files too
				JavaFiles.AddRange(from t in templates select t.SourceFile);
				JavaFiles.AddRange(from t in templates select t.DestinationFile);

				bBuildSettingsMatch = CheckDependencies(Architectures, ProjectName, ProjectDirectory, IntermediateAndroidPath,
					UnrealBuildFilesPath, GameBuildFilesPath, EngineDirectory, JavaFiles, CookFlavor, OutputPath,
					bMakeSeparateApks, bPackageDataInsideApk, bDontBundleLibrariesInAPK);

			}

			string CommandLineSourceFileName = Path.Combine(Path.GetDirectoryName(ObbFileLocation)!, Path.GetFileNameWithoutExtension(ObbFileLocation), "UECommandLine.txt");
			string CommandLineCacheFileName = Path.Combine(IntermediateAndroidPath, "UECommandLine.txt");
			if (bBuildSettingsMatch)
			{
				bool bCommandLineMatch =
					(!File.Exists(CommandLineSourceFileName) && !File.Exists(CommandLineCacheFileName)) ||
					BinaryFileEquals(CommandLineSourceFileName, CommandLineCacheFileName);
				if (!bCommandLineMatch)
				{
					bBuildSettingsMatch = false;
					Logger.LogInformation("Previous .apk file(s) were made with different stage/apk command line, forcing repackage.");
				}
			}

			// Initialize UPL contexts for each architecture enabled
			UPL.Init(NDKArches, bForDistribution, EngineDirectory, IntermediateAndroidPath, ProjectDirectory, Configuration.ToString(), bSkipGradleBuild, bPerArchBuildDir: true, ArchRemapping: ArchRemapping);
			UPL.SetGlobalContextVariable("AndroidPackageName", PackageName);

			IEnumerable<Tuple<UnrealArch, string>>? BuildList = null;

			bool bRequiresOBB = RequiresOBB(bDisallowPackagingDataInApk, ObbFileLocation);
			if (!bBuildSettingsMatch)
			{
				BuildList = from Arch in Architectures.Architectures
							let manifest = GenerateManifest(ToolChain, ProjectName, InTargetType, EngineDirectory, bForDistribution, bPackageDataInsideApk, GameBuildFilesPath, bRequiresOBB, bDisableVerifyOBBOnStartUp, Arch, CookFlavor, bUseExternalFilesDir, Configuration.ToString(), SDKLevelInt, bSkipGradleBuild, bEnableBundle)
							select Tuple.Create(Arch, manifest);
			}
			else
			{
				BuildList = from Arch in Architectures.Architectures
							let manifestFile = Path.Combine(IntermediateAndroidPath, Arch + "_AndroidManifest.xml")
							let manifest = GenerateManifest(ToolChain, ProjectName, InTargetType, EngineDirectory, bForDistribution, bPackageDataInsideApk, GameBuildFilesPath, bRequiresOBB, bDisableVerifyOBBOnStartUp, Arch, CookFlavor, bUseExternalFilesDir, Configuration.ToString(), SDKLevelInt, bSkipGradleBuild, bEnableBundle)
							let OldManifest = File.Exists(manifestFile) ? File.ReadAllText(manifestFile) : ""
							where manifest != OldManifest
							select Tuple.Create(Arch, manifest);
			}

			List<string> LLDBExtraSymbolsDirectories = new();

			// Need to do stripping here because .apk will stay the same during iteration
			if (bDontBundleLibrariesInAPK)
			{
				foreach (string NDKArch in NDKArches)
				{
					UnrealArch Arch = GetUnrealArch(NDKArch);

					string SOName = AndroidToolChain.InlineArchName(OutputPath, Arch);
					if (!File.Exists(SOName))
					{
						Logger.LogWarning("Did not find compiled .so [{SOName}]", SOName);
					}

					string SONameStripped = Path.Combine(Path.GetDirectoryName(SOName)!, Path.GetFileNameWithoutExtension(SOName) + "-stripped" + Path.GetExtension(SOName));
					StripDebugSymbols(SOName, SONameStripped, Arch, Logger, true);

					// LLDB (in Android Studio and AGDE) needs to see a file that is matching the filename on the device ("libUnreal.so") to resolve symbols,
					// otherwise it will try to download libUnreal.so and fail due to adb pull not having permissions to access it.
					// Newer lldb has "platform.plugin.remote-android.package-name" in lldb to avoid that.
					string LLDBExtraSymbolsDirectory = Path.Combine(IntermediateAndroidPath, "LLDBSymbolsLibs", Arch.ToString().ToLower());
					if (!Directory.Exists(LLDBExtraSymbolsDirectory))
					{
						Directory.CreateDirectory(LLDBExtraSymbolsDirectory);
					}
					
					string LLDBExtraSymbolsFile = Path.Combine(LLDBExtraSymbolsDirectory, "libUnreal.so");
					bool LLDBExtraSymbolsFileIsPresent = File.Exists(LLDBExtraSymbolsFile);

					string LLDBExtraSymbolsFileIsSymlinkMarkerFile = LLDBExtraSymbolsFile + ".isSymLink";
					bool LLDBExtraSymbolsFileIsSymlink = File.Exists(LLDBExtraSymbolsFileIsSymlinkMarkerFile);

					if (LLDBExtraSymbolsFileIsSymlink)
					{
						// There is no easy way to check to which location a hardlink is pointing to,
						// so to ensure we have the correct redirections let's remove the link first.
						SafeDeleteFile(LLDBExtraSymbolsFile);
					}

					// try to create symlink again if we already know that file was a symlink or it's our first time and file doesn't exist yet
					if ((LLDBExtraSymbolsFileIsSymlink || !LLDBExtraSymbolsFileIsPresent) && Utils.TryCreateSymlink(LLDBExtraSymbolsFile, SOName, Logger))
					{
						if (!LLDBExtraSymbolsFileIsSymlink)
						{
							// it's our first time creating a symlink, so create a marker file as well
							File.Create(LLDBExtraSymbolsFileIsSymlinkMarkerFile).Close();
						}
					}
					else
					{
						// copy the file if we couldn't create a symlink or file exists without the marker 
						Logger.LogWarning("Failed to create symlink '{Path}' -> '{PathToTarget}', copying the file instead", LLDBExtraSymbolsFile, SOName);
						CopyIfDifferent(SOName, LLDBExtraSymbolsFile, false, false);

						// delete the marker if it was present
						if (LLDBExtraSymbolsFileIsSymlink)
						{
							SafeDeleteFile(LLDBExtraSymbolsFileIsSymlinkMarkerFile);
						}
					}

					LLDBExtraSymbolsDirectories.Add(LLDBExtraSymbolsDirectory);
				}
			}

			// Now we have to spin over all the arch/gpu combinations to make sure they all match
			int BuildListComboTotal = BuildList.Count();
			if (BuildListComboTotal == 0)
			{
				Logger.LogInformation("Output .apk file(s) are up to date (dependencies and build settings are up to date)");
				return;
			}

			if (BypassGradlePackaging && !bEnableBundle && !bSkipGradleBuild && !bCreateFromScratch)
			{
				Logger.LogInformation("Attemping BypassGradlePackaging");
				int BuildListComboRemaining = BuildListComboTotal;

				foreach (Tuple<UnrealArch, string> build in BuildList)
				{
					UnrealArch Arch = build.Item1;
					string Manifest = build.Item2;
					string NDKArch = GetNDKArch(Arch);

					string UnrealBuildPath = Path.Combine(IntermediateAndroidPath, Arch.ToString());

					Logger.LogInformation("\n===={Time}====PREPARING NATIVE CODE====={Arch}============================================================", DateTime.Now.ToString(), Arch);

					string DestApkDirectory = Path.Combine(ProjectDirectory, "Binaries", "Android");
					string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch);
					// if the source binary was UnrealGame, replace it with the new project name, when re-packaging a binary only build
					string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UnrealGame", ProjectName);
					string DestApkName = Path.Combine(DestApkDirectory, ApkFilename + ".apk");

					// As we are always making seperate APKs we need to put the architecture into the name
					DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch);

					if (!File.Exists(SourceSOName))
					{
						throw new BuildException("Can't make an APK without the compiled .so [{0}]", SourceSOName);
					}

					if (!File.Exists(DestApkName))
					{
						Logger.LogInformation("Output .apk [{DestApkName}] does not exist, will do full packaging with Gradle", DestApkName);
						continue;
					}

					// verify the .so actually changed (this does involve an extra copy, we could always assume it changed for a little more speed)
					bool bVerifySOChanged = true;
					if (bVerifySOChanged)
					{
						string JNIDirectory = Path.Combine(UnrealBuildPath, "jni", NDKArch);
						if (!Directory.Exists(JNIDirectory))
						{
							Logger.LogInformation("JNI directory {JNIDirectory} does not exist, will do full packaging with Gradle", JNIDirectory);
							continue;
						}

						string FinalSOName = Path.Combine(JNIDirectory, "libUnreal.so");
						if (!CopyIfDifferent(SourceSOName, FinalSOName, false, false))
						{
							Logger.LogInformation("{FinalSOName} unchanged, skipping repackage of {DestApkName}", FinalSOName, DestApkName);
							BuildListComboRemaining--;
							continue;
						}
					}

					// strip symbols to make libUnreal.so small enough for packaging
					string StrippedSOName = SourceSOName + ".stripped";
					StripDebugSymbols(SourceSOName, StrippedSOName, Arch, Logger, true);

					Logger.LogInformation("\n===={Time}====PERFORMING FINAL APK PACKAGE OPERATION====={Arch}===========================================", DateTime.Now.ToString(), Arch);

					string TempAPK = Path.Combine(DestApkDirectory, "TEMP_" + Path.GetFileName(DestApkName));
					CopyAPKAndReplaceSO(Logger, DestApkName, TempAPK, StrippedSOName, "lib/" + NDKArch + "/libUnreal.so");

					// defaults for non-distribution build (debug)
					string KeyStore = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), ".android", "debug.keystore");
					string KeyStorePassword = "android";
					string KeyAlias = "androiddebugkey";
					string KeyPassword = "android";

					// read project settings for keystore information if distribution build (release)
					if (bForDistribution)
					{
						Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStore", out KeyStore);
						Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyAlias", out KeyAlias);
						Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyStorePassword", out KeyStorePassword);
						Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "KeyPassword", out KeyPassword);

						if (String.IsNullOrEmpty(KeyStore) || String.IsNullOrEmpty(KeyAlias) || String.IsNullOrEmpty(KeyStorePassword))
						{
							throw new BuildException("DistributionSigning settings are not all set. Check the DistributionSettings section in the Android tab of Project Settings");
						}

						if (String.IsNullOrEmpty(KeyPassword) || KeyPassword == "_sameaskeystore_")
						{
							KeyPassword = KeyStorePassword;
						}

						KeyStore = Path.Combine(UnrealBuildPath, KeyStore);
					}

					// Make sure the keystore file exists
					if (!File.Exists(KeyStore))
					{
						throw new BuildException("Keystore file is missing. Check the DistributionSettings section in the Android tab of Project Settings");
					}

					// sign the APK
					string BuildToolsPath = Path.Combine(Environment.ExpandEnvironmentVariables("%ANDROID_HOME%"), "Build-Tools", BuildToolsVersion);
					string APKSignerExecutable = Path.Combine(BuildToolsPath, "apksigner" + (RuntimePlatform.IsWindows ? ".bat" : ""));

					ProcessStartInfo StartInfo = new ProcessStartInfo();
					StartInfo.WorkingDirectory = IntermediateAndroidPath;
					if (RuntimePlatform.IsWindows)
					{
						StartInfo.FileName = "cmd.exe";
						StartInfo.ArgumentList.Add("/c");
						StartInfo.ArgumentList.Add(APKSignerExecutable);
						StartInfo.ArgumentList.Add("sign");
						StartInfo.ArgumentList.Add("--ks");
						StartInfo.ArgumentList.Add(KeyStore);
						StartInfo.ArgumentList.Add("--ks-pass");
						StartInfo.ArgumentList.Add("pass:" + KeyStorePassword);
						StartInfo.ArgumentList.Add("--ks-key-alias");
						StartInfo.ArgumentList.Add(KeyAlias);
						StartInfo.ArgumentList.Add("--key-pass");
						StartInfo.ArgumentList.Add("pass:" + KeyPassword);
						StartInfo.ArgumentList.Add("--out");
						StartInfo.ArgumentList.Add(DestApkName);
						StartInfo.ArgumentList.Add(TempAPK);
					}
					else
					{
						StartInfo.FileName = "/bin/sh";
						StartInfo.ArgumentList.Add("-c");
						StartInfo.ArgumentList.Add("\"" + APKSignerExecutable + "\" sign--ks \"" + KeyStore + "\" --ks-pass pass:" + KeyStorePassword + " --ks-key-alias " + KeyAlias + " --key-pass pass:" + KeyPassword + " --out \"" + DestApkName + "2\" \"" + TempAPK + "\"");
					}
					StartInfo.UseShellExecute = false;
					StartInfo.WindowStyle = ProcessWindowStyle.Minimized;

					Logger.LogInformation("Applying apksigner to apk...");

					Process Proc = new Process();
					Proc.StartInfo = StartInfo;
					Proc.Start();
					Proc.WaitForExit();

					// clean up work files
					SafeDeleteFile(TempAPK);
					SafeDeleteFile(DestApkName + ".idsig");
					SafeDeleteFile(StrippedSOName);

					if (Proc.ExitCode != 0)
					{
						string Args = "";
						foreach (string Arg in StartInfo.ArgumentList)
						{
							Args += Arg + " ";
						}
						throw new BuildException("{0} failed with args {1}", StartInfo.FileName, Args);
					}

					// copy .so with symbols if requested
					bool bBuildWithHiddenSymbolVisibility = false;
					bool bSaveSymbols = false;
					Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildWithHiddenSymbolVisibility", out bBuildWithHiddenSymbolVisibility);
					Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSaveSymbols", out bSaveSymbols);
					if (bSaveSymbols || (Configuration == UnrealTargetConfiguration.Shipping && bBuildWithHiddenSymbolVisibility))
					{
						// Copy .so with symbols to 
						int StoreVersion = GetStoreVersion(bEnableBundle ? null : Arch);
						string SymbolSODirectory = Path.Combine(DestApkDirectory, ProjectName + "_Symbols_v" + StoreVersion + "/" + ProjectName + Arch);
						string SymbolifiedSOPath = Path.Combine(SymbolSODirectory, Path.GetFileName(SourceSOName));
						MakeDirectoryIfRequired(SymbolifiedSOPath);
						Logger.LogInformation("Writing symbols to {SymbolifiedSOPath}", SymbolifiedSOPath);

						File.Copy(SourceSOName, SymbolifiedSOPath, true);
					}

					BuildListComboRemaining--;
				}

				if (BuildListComboRemaining == 0)
				{
					Logger.LogInformation("\n===={Time}====COMPLETED MAKE APK=======================================================================", DateTime.Now.ToString());
					return;
				}
			}

			// at this point, we can write out the cached build settings to compare for a next build
			File.WriteAllText(BuildSettingsCacheFile, CurrentBuildSettings);
			if (File.Exists(CommandLineSourceFileName))
			{
				CopyIfDifferent(CommandLineSourceFileName, CommandLineCacheFileName, true, true);
			}
			else
			{
				SafeDeleteFile(CommandLineCacheFileName);
			}

			// make up a dictionary of strings to replace in xml files (strings.xml)
			Dictionary<string, string> Replacements = new Dictionary<string, string>
			{
				{"${EXECUTABLE_NAME}", ApplicationDisplayName!},
				{"${PY_VISUALIZER_PATH}", Path.GetFullPath(Path.Combine(EngineDirectory, "Extras", "LLDBDataFormatters", "UEDataFormatters_2ByteChars.py"))},
				{"${IDEA_RUN_CONFIGURATION_SYMBOL_PATHS}", string.Join("", LLDBExtraSymbolsDirectories.Select(x => $"\n\t\t\t<symbol_dirs symbol_path=\"{x}\" />"))}
			};

			// steps run for each build combination (note: there should only be one GPU in future)
			foreach (Tuple<UnrealArch, string> build in BuildList)
			{
				UnrealArch Arch = build.Item1;
				string Manifest = build.Item2;
				string NDKArch = GetNDKArch(Arch);

				Logger.LogInformation("\n===={Time}====PREPARING NATIVE CODE====={Arch}============================================================", DateTime.Now.ToString(), Arch);

				string UnrealBuildPath = Path.Combine(IntermediateAndroidPath, Arch.ToString());
				string UnrealBuildGradlePath = Path.Combine(UnrealBuildPath, "gradle");

				// If we are packaging for Amazon then we need to copy the  file to the correct location
				Logger.LogInformation("bPackageDataInsideApk = {bPackageDataInsideApk}", bPackageDataInsideApk);
				if (bPackageDataInsideApk)
				{
					Logger.LogInformation("Obb location {ObbFileLocation}", ObbFileLocation);
					string ObbFileDestination = UnrealBuildPath + "/assets";
					Logger.LogInformation("Obb destination location {ObbFileDestination}", ObbFileDestination);
					if (File.Exists(ObbFileLocation))
					{
						Directory.CreateDirectory(UnrealBuildPath);
						Directory.CreateDirectory(ObbFileDestination);
						Logger.LogInformation("Obb file exists...");
						string DestFileName = Path.Combine(ObbFileDestination, "main.obb.png"); // Need a rename to turn off compression
						string SrcFileName = ObbFileLocation;
						CopyIfDifferent(SrcFileName, DestFileName, true, false);
					}
				}
				else // try to remove the file it we aren't packaging inside the APK
				{
					string ObbFileDestination = UnrealBuildPath + "/assets";
					string DestFileName = Path.Combine(ObbFileDestination, "main.obb.png");
					SafeDeleteFile(DestFileName);
				}

				// See if we need to stage a UECommandLine.txt file in assets
				string CommandLineDestFileName = Path.Combine(UnrealBuildPath, "assets", "UECommandLine.txt");
				if (File.Exists(CommandLineSourceFileName))
				{
					Directory.CreateDirectory(UnrealBuildPath);
					Directory.CreateDirectory(Path.Combine(UnrealBuildPath, "assets"));
					Console.WriteLine("UnrealCommandLine.txt exists...");
					CopyIfDifferent(CommandLineSourceFileName, CommandLineDestFileName, true, true);
				}
				else // try to remove the file if we aren't packaging one
				{
					SafeDeleteFile(CommandLineDestFileName);
				}

				// check for Android Studio project being setup for packaging the apk/debug. If this exists then we have setup and
				//Android studio project that we intend to use to finish making the APK or for debugging.
				string GradleAppImlFilename = Path.Combine(UnrealBuildGradlePath, ".idea", "modules", "app.iml");
				bool bHasAndroidStudioProject = File.Exists(GradleAppImlFilename);

				// We need to filter out copying over our gradle files and stomping the existing gradle project if we plan to use 
				// the existing AndroidStudio project to build our APK and/or Debug.
				string[]? ExcludeFolders = null;
				if (bHasAndroidStudioProject)
				{
					//Path.Combine(UnrealBuildFilesPath, "gradle")
					ExcludeFolders = new string[]{ "gradle", ".gradle", ".idea", "app", "runConfigurations" };
				}

				//Copy build files to the intermediate folder in this order (later overrides earlier):
				//	- Shared Engine
				//  - Shared Engine NoRedist (for Epic secret files)
				//  - Game
				//  - Game NoRedist (for Epic secret files)
				CopyFileDirectory(UnrealBuildFilesPath, UnrealBuildPath, Replacements, ExcludeFolders);
				CopyFileDirectory(UnrealBuildFilesPath_NFL, UnrealBuildPath, Replacements, ExcludeFolders);
				CopyFileDirectory(UnrealBuildFilesPath_NR, UnrealBuildPath, Replacements, ExcludeFolders);
				CopyFileDirectory(GameBuildFilesPath, UnrealBuildPath, Replacements, ExcludeFolders);
				CopyFileDirectory(GameBuildFilesPath_NFL, UnrealBuildPath, Replacements, ExcludeFolders);
				CopyFileDirectory(GameBuildFilesPath_NR, UnrealBuildPath, Replacements, ExcludeFolders);

				// Parse Gradle filters (may have been replaced by above copies)
				ParseFilterFile(Path.Combine(UnrealBuildPath, "GradleFilter.txt"));

				//Generate Gradle AAR dependencies
				GenerateGradleAARImports(EngineDirectory, UnrealBuildPath, NDKArches);

				//Now validate GooglePlay app_id if enabled
				ValidateGooglePlay(UnrealBuildPath);

				//determine which orientation requirements this app has
				bool bNeedLandscape = false;
				bool bNeedPortrait = false;
				DetermineScreenOrientationRequirements(NDKArches[0], out bNeedPortrait, out bNeedLandscape);

				//Now keep the splash screen images matching orientation requested
				PickSplashScreenOrientation(UnrealBuildPath, bNeedPortrait, bNeedLandscape);

				//Similarly, keep only the downloader screen image matching the orientation requested
				PickDownloaderScreenOrientation(UnrealBuildPath, bNeedPortrait, bNeedLandscape);

				// use Gradle for compile/package
				string UnrealBuildGradleAppPath = Path.Combine(UnrealBuildGradlePath, "app");
				string UnrealBuildGradleMainPath = Path.Combine(UnrealBuildGradleAppPath, "src", "main");
				string CompileSDKVersion = SDKAPILevel.Replace("android-", "");

				// Write the manifest to the correct locations (cache and real)
				String ManifestFile = Path.Combine(IntermediateAndroidPath, Arch + "_AndroidManifest.xml");
				File.WriteAllText(ManifestFile, Manifest);
				ManifestFile = Path.Combine(UnrealBuildPath, "AndroidManifest.xml");
				File.WriteAllText(ManifestFile, Manifest);

				// copy prebuild plugin files
				UPL.ProcessPluginNode(NDKArch, "prebuildCopies", "");

				XDocument AdditionalBuildPathFilesDoc = new XDocument(new XElement("files"));
				UPL.ProcessPluginNode(NDKArch, "additionalBuildPathFiles", "", ref AdditionalBuildPathFilesDoc);

				// Generate the OBBData.java file since different architectures may have different store version
				UnrealOBBDataFileName = GetUnrealJavaOBBDataFileName(Path.Combine(UnrealBuildPath, "src", PackageName.Replace('.', Path.DirectorySeparatorChar)));
				WriteJavaOBBDataFile(UnrealOBBDataFileName, PackageName, RequiredOBBFiles, CookFlavor, bPackageDataInsideApk, Arch);

				// update GameActivity.java and GameApplication.java if out of date
				UpdateGameActivity(Arch, NDKArch, EngineDirectory, UnrealBuildPath, bDontBundleLibrariesInAPK);
				UpdateGameApplication(Arch, NDKArch, EngineDirectory, UnrealBuildPath);

				// we don't actually need the SO for the bSkipGradleBuild case
				string? FinalSOName = null;
				string DestApkDirectory = Path.Combine(ProjectDirectory, "Binaries","Android");
				string? DestApkName = null;
				if (bSkipGradleBuild)
				{
					FinalSOName = OutputPath;
					if (!File.Exists(FinalSOName))
					{
						Logger.LogWarning("Did not find compiled .so [{FinalSOName}]", FinalSOName);
					}
				}
				else if (bDontBundleLibrariesInAPK)
				{
					FinalSOName = AndroidToolChain.InlineArchName(OutputPath, Arch);
					string FinalSONameStripped = Path.Combine(Path.GetDirectoryName(FinalSOName)!, Path.GetFileNameWithoutExtension(FinalSOName) + "-stripped" + Path.GetExtension(FinalSOName));
					if (!File.Exists(FinalSONameStripped))
					{
						Logger.LogWarning("Did not find compiled .so [{FinalSONameStripped}]", FinalSONameStripped);
					}

					string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UnrealGame", ProjectName);
					DestApkName = Path.Combine(DestApkDirectory, ApkFilename + ".apk");

					// As we are always making seperate APKs we need to put the architecture into the name
					DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch);

					bool bUseAFS = true;
					if (!Ini.GetBool("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "bEnablePlugin", out bUseAFS))
					{
						bUseAFS = true;
					}

					Ini.GetString("/Script/AndroidFileServerEditor.AndroidFileServerRuntimeSettings", "SecurityToken", out string AFSToken);

					AFSToken = string.IsNullOrEmpty(AFSToken) ? "" : " -k " + AFSToken;

					string AFSExecutable = Path.Combine(Unreal.EngineDirectory.ToString(), @"Binaries/DotNET/Android/UnrealAndroidFileTool", GetAFSExecutable(UnrealTargetPlatform.Win64, Logger));
					string AFSArguments = $"-p {PackageName}{AFSToken}";

					string? SOPushScriptLocation = Path.GetDirectoryName(FinalSOName)!;

					string FinalSONameStrippedRelative = Path.GetRelativePath(SOPushScriptLocation, FinalSONameStripped);

					// MakeApk will not be called in bDontBundleLibrariesInAPK mode, so we need to run stripping outside of it
					string SOPushScript = @$"
set ADB=adb
set AFS={AFSExecutable.Replace("/", "\\")}
set AFSARGS={AFSArguments}
set DEVICE=
if not \""%1\""==\""\"" set DEVICE=-s %1
pushd %~dp0
";
					if (bUseAFS)
					{
						SOPushScript += @$"
%AFS% %DEVICE% %AFSARGS% push {FinalSONameStrippedRelative} ""^int/libUnreal.so""
if ""%ERRORLEVEL%"" NEQ ""0"" (exit /b %ERRORLEVEL%)
";
					}
					else
					{
						SOPushScript += @$"
%ADB% %DEVICE% push -z lz4 {FinalSONameStrippedRelative} /data/local/tmp/{FinalSONameStrippedRelative}
if ""%ERRORLEVEL%"" NEQ ""0"" (%ADB% %DEVICE% push {FinalSONameStrippedRelative} /data/local/tmp/{FinalSONameStrippedRelative})
if ""%ERRORLEVEL%"" NEQ ""0"" (exit /b %ERRORLEVEL%)
%ADB% %DEVICE% shell run-as {PackageName} mkdir -p ./files
%ADB% %DEVICE% shell run-as {PackageName} cp /data/local/tmp/{FinalSONameStrippedRelative} ./files/libUnreal.so
if ""%ERRORLEVEL%"" NEQ ""0"" (exit /b %ERRORLEVEL%)
%ADB% %DEVICE% shell rm /data/local/tmp/{FinalSONameStrippedRelative}
";
					}

					SOPushScript +=
@"
popd
";
					// name is also used in the AGDE project, see AndroidProjectGenerator.cs
					string SOPushScriptName = Path.Combine(Path.GetDirectoryName(FinalSOName)!, "Push_" + Path.GetFileNameWithoutExtension(FinalSOName) + "_so.bat");
					File.WriteAllText(SOPushScriptName, SOPushScript);

					// Remove libUnreal.so from gradle project if any
					string JniDir = UnrealBuildPath + "/jni/" + NDKArch;
					string TempFinalSOName = JniDir + "/libUnreal.so";
					SafeDeleteFile(TempFinalSOName);
				}
				else
				{
					string SourceSOName = AndroidToolChain.InlineArchName(OutputPath, Arch);
					// if the source binary was UnrealGame, replace it with the new project name, when re-packaging a binary only build
					string ApkFilename = Path.GetFileNameWithoutExtension(OutputPath).Replace("UnrealGame", ProjectName);
					DestApkName = Path.Combine(DestApkDirectory, ApkFilename + ".apk");

					// As we are always making seperate APKs we need to put the architecture into the name
					DestApkName = AndroidToolChain.InlineArchName(DestApkName, Arch);

					if (!File.Exists(SourceSOName))
					{
						throw new BuildException("Can't make an APK without the compiled .so [{0}]", SourceSOName);
					}
					if (!Directory.Exists(UnrealBuildPath + "/jni"))
					{
						throw new BuildException("Can't make an APK without the jni directory [{0}/jni]", UnrealBuildFilesPath);
					}
					
					// Delete the push script if exists
					string SOPushScriptName = Path.Combine(Path.GetDirectoryName(DestApkName)!, "Push_" + Path.GetFileNameWithoutExtension(DestApkName) + "_so.bat");
					SafeDeleteFile(SOPushScriptName);

					string JniDir = UnrealBuildPath + "/jni/" + NDKArch;
					FinalSOName = JniDir + "/libUnreal.so";

					// clear out libs directory like ndk-build would have
					string LibsDir = Path.Combine(UnrealBuildPath, "libs");
					DeleteDirectory(LibsDir, Logger);
					MakeDirectoryIfRequired(LibsDir);

					// check to see if libUnreal.so needs to be copied
					if (BuildListComboTotal > 1 || FilesAreDifferent(SourceSOName, FinalSOName))
					{
						Logger.LogInformation("\nCopying new .so {SourceSOName} file to jni folder...", SourceSOName);
						Directory.CreateDirectory(JniDir);
						// copy the binary to the standard .so location
						File.Copy(SourceSOName, FinalSOName, true);
						File.SetLastWriteTimeUtc(FinalSOName, File.GetLastWriteTimeUtc(SourceSOName));
					}

					// remove any read only flags
					FileInfo DestFileInfo = new FileInfo(FinalSOName);
					DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
					File.SetLastWriteTimeUtc(FinalSOName, File.GetLastWriteTimeUtc(SourceSOName));
				}

				bool bSkipLibCpp = false;
				Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSkipLibCpp", out bSkipLibCpp);
				if (!bSkipLibCpp)
				{
					// after ndk-build is called, we can now copy in the stl .so (ndk-build deletes old files)
					// copy libc++_shared.so to library
					CopySTL(ToolChain, UnrealBuildPath, Arch, NDKArch, bForDistribution);
				}
				CopyPSOService(UnrealBuildPath, UnrealPreBuiltFilesPath, Arch, NDKArch);
				CopyGfxDebugger(UnrealBuildPath, Arch, NDKArch);
				CopyVulkanValidationLayers(UnrealBuildPath, Arch, NDKArch, Configuration.ToString());

				if (Sanitizer != AndroidToolChain.ClangSanitizer.None)
				{
					CopyClangSanitizerLib(ToolChain, UnrealBuildPath, Arch, NDKArch, Sanitizer);

					if (bEnableScudoMemoryTracing)
					{
						throw new BuildException("ScudoMemoryTrace not supported with sanitizer enabled");
					}
				}
				else if (bEnableScudoMemoryTracing)
				{
					CopyScudoMemoryTracerLib(UnrealBuildPath, Arch, NDKArch, PackageName);
				}

				// copy postbuild plugin files
				UPL.ProcessPluginNode(NDKArch, "resourceCopies", "");

				CreateAdditonalBuildPathFiles(NDKArch, UnrealBuildPath, AdditionalBuildPathFilesDoc);

				Logger.LogInformation("\n===={Time}====PERFORMING FINAL APK PACKAGE OPERATION====={Arch}===========================================", DateTime.Now.ToString(), Arch);

				// check if any plugins want to increase the required compile SDK version
				string CompileSDKMin = UPL.ProcessPluginNode(NDKArch, "minimumSDKAPI", "");
				if (!String.IsNullOrEmpty(CompileSDKMin))
				{
					int CompileSDKVersionInt;
					if (!Int32.TryParse(CompileSDKVersion, out CompileSDKVersionInt))
					{
						CompileSDKVersionInt = 23;
					}

					bool bUpdatedCompileSDK = false;
					string[] CompileSDKLines = CompileSDKMin.Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);
					foreach (string CompileLine in CompileSDKLines)
					{
						//string VersionString = CompileLine.Replace("android-", "");
						int VersionInt;
						if (Int32.TryParse(CompileLine, out VersionInt))
						{
							if (VersionInt > CompileSDKVersionInt)
							{
								CompileSDKVersionInt = VersionInt;
								bUpdatedCompileSDK = true;
							}
						}
					}

					if (bUpdatedCompileSDK)
					{
						CompileSDKVersion = CompileSDKVersionInt.ToString();
						Logger.LogInformation("Building Java with SDK API Level 'android-{CompileSDKVersion}' due to enabled plugin requirements", CompileSDKVersion);
					}
				}

				// stage files into gradle app directory
				string GradleManifest = Path.Combine(UnrealBuildGradleMainPath, "AndroidManifest.xml");
				MakeDirectoryIfRequired(GradleManifest);
				CopyIfDifferent(Path.Combine(UnrealBuildPath, "AndroidManifest.xml"), GradleManifest, true, true);

				string[] Excludes;
				switch (NDKArch)
				{
					default:
					case "arm64-v8a":
						Excludes = new string[] { "armeabi-v7a", "x86", "x86-64" };
						break;

					case "x86_64":
						Excludes = new string[] { "armeabi-v7a", "arm64-v8a", "x86" };
						break;
				}

				CleanCopyDirectory(Path.Combine(UnrealBuildPath, "jni"), Path.Combine(UnrealBuildGradleMainPath, "jniLibs"), Excludes);  // has debug symbols
				CleanCopyDirectory(Path.Combine(UnrealBuildPath, "libs"), Path.Combine(UnrealBuildGradleMainPath, "libs"), Excludes);
				if ((Sanitizer != AndroidToolChain.ClangSanitizer.None && (Sanitizer != AndroidToolChain.ClangSanitizer.HwAddress || ToolChain.HasEmbeddedHWASanSupport())) || bEnableScudoMemoryTracing)
				{
					CleanCopyDirectory(Path.Combine(UnrealBuildPath, "resources"), Path.Combine(UnrealBuildGradleMainPath, "resources"), Excludes);
				}

				CleanCopyDirectory(Path.Combine(UnrealBuildPath, "assets"), Path.Combine(UnrealBuildGradleMainPath, "assets"));
				CleanCopyDirectory(Path.Combine(UnrealBuildPath, "res"), Path.Combine(UnrealBuildGradleMainPath, "res"));
				CleanCopyDirectory(Path.Combine(UnrealBuildPath, "src"), Path.Combine(UnrealBuildGradleMainPath, "java"));

				// do any plugin requested copies
				UPL.ProcessPluginNode(NDKArch, "gradleCopies", "");

				// get min and target SDK versions
				int MinSDKVersion = 0;
				int TargetSDKVersion = 0;
				int NDKLevelInt = 0;
				GetMinTargetSDKVersions(ToolChain, Arch, UPL, NDKArch, bEnableBundle, out MinSDKVersion, out TargetSDKVersion, out NDKLevelInt);

				// move JavaLibs into subprojects
				string JavaLibsDir = Path.Combine(UnrealBuildPath, "JavaLibs");
				PrepareJavaLibsForGradle(JavaLibsDir, UnrealBuildGradlePath, MinSDKVersion.ToString(), TargetSDKVersion.ToString(), CompileSDKVersion, BuildToolsVersion, NDKArch);

				// Create local.properties
				String LocalPropertiesFilename = Path.Combine(UnrealBuildGradlePath, "local.properties");
				StringBuilder LocalProperties = new StringBuilder();
				//				LocalProperties.AppendLine(string.Format("ndk.dir={0}", Environment.GetEnvironmentVariable("NDKROOT")!.Replace("\\", "/")));
				LocalProperties.AppendLine(String.Format("sdk.dir={0}", Environment.GetEnvironmentVariable("ANDROID_HOME")!.Replace("\\", "/")));
				File.WriteAllText(LocalPropertiesFilename, LocalProperties.ToString());

				CreateGradlePropertiesFiles(ToolChain, Arch, MinSDKVersion, TargetSDKVersion, CompileSDKVersion, BuildToolsVersion, PackageName, DestApkName, NDKArch,
					UnrealBuildFilesPath, GameBuildFilesPath, UnrealBuildGradleAppPath, UnrealBuildPath, UnrealBuildGradlePath, bForDistribution, bSkipGradleBuild, RequiredOBBFiles);

				if (!bSkipGradleBuild)
				{
					string GradleScriptPath = Path.Combine(UnrealBuildGradlePath, "gradlew");
					if (!RuntimePlatform.IsWindows)
					{
						// fix permissions for Mac/Linux
						RunCommandLineProgramWithException(UnrealBuildGradlePath, "/bin/sh", String.Format("-c 'chmod 0755 \"{0}\"'", GradleScriptPath.Replace("'", "'\"'\"'")), Logger, "Fix gradlew permissions");
					}
					else
					{
						if (CreateRunGradle(UnrealBuildGradlePath))
						{
							GradleScriptPath = Path.Combine(UnrealBuildGradlePath, "rungradle.bat");
						}
						else
						{
							GradleScriptPath = Path.Combine(UnrealBuildGradlePath, "gradlew.bat");
						}
					}

					if (!bEnableBundle)
					{
						string GradleBuildType = bForDistribution ? ":app:assembleRelease" : ":app:assembleDebug";

						// collect optional additional Gradle parameters from plugins
						string GradleOptions = UPL.ProcessPluginNode(NDKArch, "gradleParameters", GradleBuildType); //  "--stacktrace --debug " + GradleBuildType);
						string GradleSecondCallOptions = UPL.ProcessPluginNode(NDKArch, "gradleSecondCallParameters", "");

						// check for Android Studio project, call Gradle if doesn't exist (assume user will build with Android Studio)
						if (!bHasAndroidStudioProject)
						{
							// make sure destination exists
							Directory.CreateDirectory(Path.GetDirectoryName(DestApkName)!);

							// Use gradle to build the .apk file
							string ShellExecutable = RuntimePlatform.IsWindows ? "cmd.exe" : "/bin/sh";
							string ShellParametersBegin = RuntimePlatform.IsWindows ? "/c " : "-c '";
							string ShellParametersEnd = RuntimePlatform.IsWindows ? "" : "'";
							RunCommandLineProgramWithExceptionAndFiltering(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, Logger, "Making .apk with Gradle...");

							if (!String.IsNullOrEmpty(GradleSecondCallOptions))
							{
								RunCommandLineProgramWithExceptionAndFiltering(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleSecondCallOptions + ShellParametersEnd, Logger, "Additional Gradle steps...");
							}

							// For build machine run a clean afterward to clean up intermediate files (does not remove final APK)
							if (bIsBuildMachine)
							{
								//GradleOptions = "tasks --all";
								//RunCommandLineProgramWithException(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, "Listing all tasks...");

								// on Windows sometimes minifyReleaseWithR8 is keeping a lock on classes.dex so kill java.exe
								if (RuntimePlatform.IsWindows)
								{
									RunCommandLineProgramAndReturnResult(UnrealBuildGradlePath, ShellExecutable, "/c taskkill.exe /F /IM java.exe /T", Logger, "Terminate java.exe processes");
								}

								GradleOptions = "clean";
								RunCommandLineProgramWithExceptionAndFiltering(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, Logger, "Cleaning Gradle intermediates...");
							}
						}
						else
						{
							Logger.LogInformation("=============================================================================================");
							Logger.LogInformation("Android Studio project found, skipping Gradle; complete creation of APK in Android Studio!!!!");
							Logger.LogInformation("Delete '{GradleAppImlFilename} if you want to have UnrealBuildTool run Gradle for future runs.", GradleAppImlFilename);
							Logger.LogInformation("=============================================================================================");
						}
					}
				}

				bool bBuildWithHiddenSymbolVisibility = false;
				bool bSaveSymbols = false;

				Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildWithHiddenSymbolVisibility", out bBuildWithHiddenSymbolVisibility);
				Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSaveSymbols", out bSaveSymbols);
				bSaveSymbols = true;
				if (bSaveSymbols || (Configuration == UnrealTargetConfiguration.Shipping && bBuildWithHiddenSymbolVisibility))
				{
					// Copy .so with symbols to 
					int StoreVersion = GetStoreVersion(bEnableBundle ? null : Arch);
					string SymbolSODirectory = Path.Combine(DestApkDirectory, ProjectName + "_Symbols_v" + StoreVersion, ProjectName + Arch);
					string SymbolifiedSOPath = Path.Combine(SymbolSODirectory, Path.GetFileName(FinalSOName));
					MakeDirectoryIfRequired(SymbolifiedSOPath);
					Logger.LogInformation("Writing symbols to {SymbolifiedSOPath}", SymbolifiedSOPath);

					File.Copy(FinalSOName, SymbolifiedSOPath, true);
				}
			}

			// Deal with generating an App Bundle
			if (bEnableBundle && !bSkipGradleBuild)
			{
				bool bCombinedBundleOK = true;

				// try to make a combined Gradle project for all architectures (may fail if incompatible configuration)
				string UnrealGradleDest = Path.Combine(IntermediateAndroidPath, "gradle");

				// start fresh each time for now
				DeleteDirectory(UnrealGradleDest, Logger);

				// make sure destination exists
				Directory.CreateDirectory(UnrealGradleDest);

				String ABIFilter = "";

				// loop through and merge the different architecture gradle directories
				foreach (Tuple<UnrealArch, string> build in BuildList)
				{
					UnrealArch Arch = build.Item1;
					string Manifest = build.Item2;
					string NDKArch = GetNDKArch(Arch);

					string UnrealBuildPath = Path.Combine(IntermediateAndroidPath, Arch.ToString());
					string UnrealBuildGradlePath = Path.Combine(UnrealBuildPath, "gradle");

					if (!Directory.Exists(UnrealBuildGradlePath))
					{
						Logger.LogInformation("Source directory missing: {GradlePath}", UnrealBuildGradlePath);
						bCombinedBundleOK = false;
						break;
					}

					ABIFilter += ", \"" + NDKArch + "\"";

					int UnrealBuildGradlePathLength = UnrealBuildGradlePath.Length;
					string[] SourceFiles = Directory.GetFiles(UnrealBuildGradlePath, "*.*", SearchOption.AllDirectories);
					foreach (string Filename in SourceFiles)
					{
						// make the dest filename with the same structure as it was in SourceDir
						string DestFilename = Path.Combine(UnrealGradleDest, Utils.MakePathRelativeTo(Filename, UnrealBuildGradlePath));

						// skip the build directories
						string Workname = Filename.Replace("\\", "/");
						string DirectoryName = Path.GetDirectoryName(Filename)!.Substring(UnrealBuildGradlePathLength);
						if (DirectoryName.Contains("build") || Workname.Contains("/."))
						{
							continue;
						}

						// if destination doesn't exist, just copy it
						if (!File.Exists(DestFilename))
						{
							string DestSubdir = Path.GetDirectoryName(DestFilename)!;
							if (!Directory.Exists(DestSubdir))
							{
								Directory.CreateDirectory(DestSubdir);
							}

							// copy it
							File.Copy(Filename, DestFilename);

							// preserve timestamp and clear read-only flags
							FileInfo DestFileInfo = new FileInfo(DestFilename);
							DestFileInfo.Attributes = DestFileInfo.Attributes & ~FileAttributes.ReadOnly;
							File.SetLastWriteTimeUtc(DestFilename, File.GetLastWriteTimeUtc(Filename));

							Logger.LogInformation("Copied file {DestFilename}.", DestFilename);
							continue;
						}

						if (FilesAreIdentical(Filename, DestFilename))
						{
							continue;
						}

						// ignore abi.gradle, we're going to generate a new one
						if (Filename.EndsWith("abi.gradle"))
						{
							continue;
						}

						// ignore OBBData.java, we won't use it
						if (Filename.EndsWith("OBBData.java"))
						{
							continue;
						}

						// deal with AndroidManifest.xml
						if (Filename.EndsWith("AndroidManifest.xml"))
						{
							// only allowed to differ by versionCode
							string[] SourceManifest = File.ReadAllLines(Filename);
							string[] DestManifest = File.ReadAllLines(DestFilename);

							if (SourceManifest.Length == DestManifest.Length)
							{
								bool bDiffers = false;
								for (int Index = 0; Index < SourceManifest.Length; Index++)
								{
									if (SourceManifest[Index] == DestManifest[Index])
									{
										continue;
									}

									int SourceVersionIndex = SourceManifest[Index].IndexOf("android:versionCode=");
									if (SourceVersionIndex < 0)
									{
										bDiffers = true;
										break;
									}

									int DestVersionIndex = DestManifest[Index].IndexOf("android:versionCode=");
									if (DestVersionIndex < 0)
									{
										bDiffers = true;
										break;
									}

									int SourceVersionIndex2 = SourceManifest[Index].Substring(SourceVersionIndex + 22).IndexOf("\"");
									string FixedSource = SourceManifest[Index].Substring(0, SourceVersionIndex + 21) + SourceManifest[Index].Substring(SourceVersionIndex + 22 + SourceVersionIndex2);

									int DestVersionIndex2 = DestManifest[Index].Substring(DestVersionIndex + 22).IndexOf("\"");
									string FixedDest = SourceManifest[Index].Substring(0, DestVersionIndex + 21) + DestManifest[Index].Substring(DestVersionIndex + 22 + DestVersionIndex2);

									if (FixedSource != FixedDest)
									{
										bDiffers = true;
										break;
									}
								}
								if (!bDiffers)
								{
									continue;
								}
							}

							// differed too much
							bCombinedBundleOK = false;
							Logger.LogInformation("AndroidManifest.xml files differ too much to combine for single AAB: '{Filename}' != '{DestFilename}'", Filename, DestFilename);
							break;
						}

						// deal with buildAdditions.gradle
						if (Filename.EndsWith("buildAdditions.gradle"))
						{
							// allow store filepath to differ
							string[] SourceProperties = File.ReadAllLines(Filename);
							string[] DestProperties = File.ReadAllLines(DestFilename);

							if (SourceProperties.Length == DestProperties.Length)
							{
								bool bDiffers = false;
								for (int Index = 0; Index < SourceProperties.Length; Index++)
								{
									if (SourceProperties[Index] == DestProperties[Index])
									{
										continue;
									}

									if (SourceProperties[Index].Contains("storeFile file(") && DestProperties[Index].Contains("storeFile file("))
									{
										continue;
									}

									bDiffers = true;
									break;
								}
								if (!bDiffers)
								{
									continue;
								}
							}

							// differed too much
							bCombinedBundleOK = false;
							Logger.LogInformation("buildAdditions.gradle files differ too much to combine for single AAB: '{Filename}' != '{DestFilename}'", Filename, DestFilename);
							break;
						}

						// deal with gradle.properties
						if (Filename.EndsWith("gradle.properties"))
						{
							// allow STORE_VERSION and OUTPUT_FILENAME to differ
							// only allowed to differ by versionCode
							string[] SourceProperties = File.ReadAllLines(Filename);
							string[] DestProperties = File.ReadAllLines(DestFilename);

							if (SourceProperties.Length == DestProperties.Length)
							{
								bool bDiffers = false;
								for (int Index = 0; Index < SourceProperties.Length; Index++)
								{
									if (SourceProperties[Index] == DestProperties[Index])
									{
										continue;
									}

									if (SourceProperties[Index].StartsWith("STORE_VERSION=") && DestProperties[Index].StartsWith("STORE_VERSION="))
									{
										continue;
									}

									if (SourceProperties[Index].StartsWith("STORE_FILE=") && DestProperties[Index].StartsWith("STORE_FILE="))
									{
										continue;
									}

									if (SourceProperties[Index].StartsWith("OUTPUT_FILENAME=") && DestProperties[Index].StartsWith("OUTPUT_FILENAME="))
									{
										continue;
									}

									if (SourceProperties[Index].StartsWith("OUTPUT_BUNDLEFILENAME=") && DestProperties[Index].StartsWith("OUTPUT_BUNDLEFILENAME="))
									{
										continue;
									}

									if (SourceProperties[Index].StartsWith("OUTPUT_UNIVERSALFILENAME=") && DestProperties[Index].StartsWith("OUTPUT_UNIVERSALFILENAME="))
									{
										continue;
									}

									bDiffers = true;
									break;
								}
								if (!bDiffers)
								{
									continue;
								}
							}

							// differed too much
							bCombinedBundleOK = false;
							Logger.LogInformation("gradle.properties files differ too much to combine for single AAB: '{Filename}' != '{DestFilename}'", Filename, DestFilename);
							break;
						}

						// there are unknown differences, cannot make a single AAB
						bCombinedBundleOK = false;
						Logger.LogInformation("Gradle projects differ too much to combine for single AAB: '{Filename}' != '{DestFilename}'", Filename, DestFilename);
						break;
					}
				}

				if (bCombinedBundleOK)
				{
					string NDKArch = NDKArches[0];

					string UnrealBuildGradlePath = UnrealGradleDest;

					// write a new abi.gradle
					StringBuilder ABIGradle = new StringBuilder();
					ABIGradle.AppendLine("android {");
					ABIGradle.AppendLine("\tdefaultConfig {");
					ABIGradle.AppendLine("\t\tndk {");
					ABIGradle.AppendLine(String.Format("\t\t\tabiFilters{0}", ABIFilter.Substring(1)));
					ABIGradle.AppendLine("\t\t}");
					ABIGradle.AppendLine("\t}");
					ABIGradle.AppendLine("}");
					string ABIGradleFilename = Path.Combine(UnrealGradleDest, "app", "abi.gradle");
					File.WriteAllText(ABIGradleFilename, ABIGradle.ToString());

					// update manifest to use versionCode properly
					string BaseStoreVersion = GetStoreVersion(null).ToString();
					string ManifestFilename = Path.Combine(UnrealBuildGradlePath, "app", "src", "main", "AndroidManifest.xml");
					string[] ManifestContents = File.ReadAllLines(ManifestFilename);
					for (int Index = 0; Index < ManifestContents.Length; Index++)
					{
						int ManifestVersionIndex = ManifestContents[Index].IndexOf("android:versionCode=");
						if (ManifestVersionIndex < 0)
						{
							continue;
						}

						int ManifestVersionIndex2 = ManifestContents[Index].Substring(ManifestVersionIndex + 22).IndexOf("\"");
						ManifestContents[Index] = ManifestContents[Index].Substring(0, ManifestVersionIndex + 21) + BaseStoreVersion + ManifestContents[Index].Substring(ManifestVersionIndex + 22 + ManifestVersionIndex2);
						break;
					}
					File.WriteAllLines(ManifestFilename, ManifestContents);

					bool bEnableUniversalAPK = false;
					Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableUniversalAPK", out bEnableUniversalAPK);

					// update gradle.properties to set STORE_VERSION properly, and OUTPUT_BUNDLEFILENAME
					string GradlePropertiesFilename = Path.Combine(UnrealBuildGradlePath, "gradle.properties");
					string GradlePropertiesContent = File.ReadAllText(GradlePropertiesFilename);
					GradlePropertiesContent += String.Format("\nSTORE_VERSION={0}\nOUTPUT_BUNDLEFILENAME={1}\n", BaseStoreVersion,
						Path.GetFileNameWithoutExtension(OutputPath).Replace("UnrealGame", ProjectName) + ".aab");
					if (bEnableUniversalAPK)
					{
						GradlePropertiesContent += String.Format("OUTPUT_UNIVERSALFILENAME={0}\n",
							Path.GetFileNameWithoutExtension(OutputPath).Replace("UnrealGame", ProjectName) + "_universal.apk");
					}
					File.WriteAllText(GradlePropertiesFilename, GradlePropertiesContent);

					string GradleScriptPath = Path.Combine(UnrealBuildGradlePath, "gradlew");
					if (!RuntimePlatform.IsWindows)
					{
						// fix permissions for Mac/Linux
						RunCommandLineProgramWithException(UnrealBuildGradlePath, "/bin/sh", String.Format("-c 'chmod 0755 \"{0}\"'", GradleScriptPath.Replace("'", "'\"'\"'")), Logger, "Fix gradlew permissions");
					}
					else
					{
						if (CreateRunGradle(UnrealBuildGradlePath))
						{
							GradleScriptPath = Path.Combine(UnrealBuildGradlePath, "rungradle.bat");
						}
						else
						{
							GradleScriptPath = Path.Combine(UnrealBuildGradlePath, "gradlew.bat");
						}
					}

					string GradleBuildType = bForDistribution ? ":app:bundleRelease" : ":app:bundleDebug";

					// collect optional additional Gradle parameters from plugins
					string GradleOptions = UPL.ProcessPluginNode(NDKArch, "gradleParameters", GradleBuildType); //  "--stacktrace --debug " + GradleBuildType);
					string GradleSecondCallOptions = UPL.ProcessPluginNode(NDKArch, "gradleSecondCallParameters", "");

					// make sure destination exists
					//Directory.CreateDirectory(Path.GetDirectoryName(DestApkName));

					// Use gradle to build the .apk file
					string ShellExecutable = RuntimePlatform.IsWindows ? "cmd.exe" : "/bin/sh";
					string ShellParametersBegin = RuntimePlatform.IsWindows ? "/c " : "-c '";
					string ShellParametersEnd = RuntimePlatform.IsWindows ? "" : "'";
					RunCommandLineProgramWithExceptionAndFiltering(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, Logger, "Making .aab with Gradle...");

					if (!String.IsNullOrEmpty(GradleSecondCallOptions))
					{
						RunCommandLineProgramWithExceptionAndFiltering(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleSecondCallOptions + ShellParametersEnd, Logger, "Additional Gradle steps...");
					}

					// For build machine run a clean afterward to clean up intermediate files (does not remove final APK)
					if (bIsBuildMachine)
					{
						//GradleOptions = "tasks --all";
						//RunCommandLineProgramWithException(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, "Listing all tasks...");

						GradleOptions = "clean";
						RunCommandLineProgramWithExceptionAndFiltering(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, Logger, "Cleaning Gradle intermediates...");
					}
				}
				else
				{
					// generate an AAB for each architecture separately, was unable to merge
					foreach (Tuple<UnrealArch, string> build in BuildList)
					{
						UnrealArch Arch = build.Item1;
						string Manifest = build.Item2;
						string NDKArch = GetNDKArch(Arch);

						Logger.LogInformation("\n===={Time}====GENERATING BUNDLE====={Arch}================================================================", DateTime.Now.ToString(), Arch);

						string UnrealBuildPath = Path.Combine(IntermediateAndroidPath, Arch.ToString());
						string UnrealBuildGradlePath = Path.Combine(UnrealBuildPath, "gradle");

						string GradleScriptPath = Path.Combine(UnrealBuildGradlePath, "gradlew");
						if (!RuntimePlatform.IsWindows)
						{
							// fix permissions for Mac/Linux
							RunCommandLineProgramWithException(UnrealBuildGradlePath, "/bin/sh", String.Format("-c 'chmod 0755 \"{0}\"'", GradleScriptPath.Replace("'", "'\"'\"'")), Logger, "Fix gradlew permissions");
						}
						else
						{
							if (CreateRunGradle(UnrealBuildGradlePath))
							{
								GradleScriptPath = Path.Combine(UnrealBuildGradlePath, "rungradle.bat");
							}
							else
							{
								GradleScriptPath = Path.Combine(UnrealBuildGradlePath, "gradlew.bat");
							}
						}

						string GradleBuildType = bForDistribution ? ":app:bundleRelease" : ":app:bundleDebug";

						// collect optional additional Gradle parameters from plugins
						string GradleOptions = UPL.ProcessPluginNode(NDKArch, "gradleParameters", GradleBuildType); //  "--stacktrace --debug " + GradleBuildType);
						string GradleSecondCallOptions = UPL.ProcessPluginNode(NDKArch, "gradleSecondCallParameters", "");

						// make sure destination exists
						//Directory.CreateDirectory(Path.GetDirectoryName(DestApkName));

						// Use gradle to build the .apk file
						string ShellExecutable = RuntimePlatform.IsWindows ? "cmd.exe" : "/bin/sh";
						string ShellParametersBegin = RuntimePlatform.IsWindows ? "/c " : "-c '";
						string ShellParametersEnd = RuntimePlatform.IsWindows ? "" : "'";
						RunCommandLineProgramWithExceptionAndFiltering(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, Logger, "Making .aab with Gradle...");

						if (!String.IsNullOrEmpty(GradleSecondCallOptions))
						{
							RunCommandLineProgramWithExceptionAndFiltering(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleSecondCallOptions + ShellParametersEnd, Logger, "Additional Gradle steps...");
						}

						// For build machine run a clean afterward to clean up intermediate files (does not remove final APK)
						if (bIsBuildMachine)
						{
							//GradleOptions = "tasks --all";
							//RunCommandLineProgramWithException(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, "Listing all tasks...");

							GradleOptions = "clean";
							RunCommandLineProgramWithExceptionAndFiltering(UnrealBuildGradlePath, ShellExecutable, ShellParametersBegin + "\"" + GradleScriptPath + "\" " + GradleOptions + ShellParametersEnd, Logger, "Cleaning Gradle intermediates...");
						}
					}
				}
			}

			Logger.LogInformation("\n===={Time}====COMPLETED MAKE APK=======================================================================", DateTime.Now.ToString());
		}

		public static string GetAFSExecutable(UnrealTargetPlatform Target, ILogger Logger)
		{
			if (Target == UnrealTargetPlatform.Win64)
			{
				return "win-x64/UnrealAndroidFileTool.exe";
			}
			if (Target == UnrealTargetPlatform.Mac)
			{
				return "osx-x64/UnrealAndroidFileTool";
			}
			if (Target == UnrealTargetPlatform.Linux)
			{
				return "linux-x64/UnrealAndroidFileTool";
			}
			Logger.LogWarning("GetAFSExecutable unsupported target, assuming Win64");
			return "win-x64/UnrealAndroidFileTool.exe";
		}

		private List<string> CollectPluginDataPaths(TargetReceipt Receipt)
		{
			List<string> PluginExtras = new List<string>();
			if (Receipt == null)
			{
				Logger.LogInformation("Receipt is NULL");
				return PluginExtras;
			}

			// collect plugin extra data paths from target receipt
			IEnumerable<ReceiptProperty> Results = Receipt.AdditionalProperties.Where(x => x.Name == "AndroidPlugin");
			foreach (ReceiptProperty Property in Results)
			{
				// Keep only unique paths
				string PluginPath = Property.Value;
				if (PluginExtras.FirstOrDefault(x => x == PluginPath) == null)
				{
					PluginExtras.Add(PluginPath);
					Logger.LogInformation("AndroidPlugin: {PluginPath}", PluginPath);
				}
			}
			return PluginExtras;
		}

		public override bool PrepTargetForDeployment(TargetReceipt Receipt)
		{
			//Logger.LogInformation("$$$$$$$$$$$$$$ PrepTargetForDeployment $$$$$$$$$$$$$$$$$");

			DirectoryReference ProjectDirectory = DirectoryReference.FromFile(Receipt.ProjectFile) ?? Unreal.EngineDirectory;
			string TargetName = (Receipt.ProjectFile == null ? Receipt.TargetName : Receipt.ProjectFile.GetFileNameWithoutAnyExtensions());

			AndroidToolChain ToolChain = (AndroidToolChain)((AndroidPlatform)UEBuildPlatform.GetBuildPlatform(Receipt.Platform)).CreateTempToolChainForProject(Receipt.ProjectFile);

			// get the receipt
			SetAndroidPluginData(Receipt.Architectures, CollectPluginDataPaths(Receipt));

			bool bShouldCompileAsDll = Receipt.HasValueForAdditionalProperty("CompileAsDll", "true");

			SavePackageInfo(TargetName, ProjectDirectory.FullName, Receipt.TargetType, bShouldCompileAsDll);

			// Get the output paths
			BuildProductType ProductType = bShouldCompileAsDll ? BuildProductType.DynamicLibrary : BuildProductType.Executable;
			List<FileReference> OutputPaths = Receipt.BuildProducts.Where(x => x.Type == ProductType).Select(x => x.Path).ToList();
			if (OutputPaths.Count < 1)
			{
				throw new BuildException("Target file does not contain either executable or dynamic library .so");
			}

			// we need to strip architecture from any of the output paths
			string BaseSoName = ToolChain.RemoveArchName(OutputPaths[0].FullName);

			// make an apk at the end of compiling, so that we can run without packaging (debugger, cook on the fly, etc)
			string RelativeEnginePath = Unreal.EngineDirectory.MakeRelativeTo(DirectoryReference.GetCurrentDirectory());

			MakeApk(ToolChain, TargetName, Receipt.TargetType, ProjectDirectory.FullName, BaseSoName, RelativeEnginePath, bForDistribution: false, CookFlavor: "", Configuration: Receipt.Configuration,
				bMakeSeparateApks: ShouldMakeSeparateApks(), bIncrementalPackage: true, bDisallowPackagingDataInApk: false, bDisallowExternalFilesDir: true, bSkipGradleBuild: bShouldCompileAsDll, bIsArchive: false, bIsFromUAT: false);

			// if we made any non-standard .apk files, the generated debugger settings may be wrong
			if (ShouldMakeSeparateApks() && (OutputPaths.Count > 1 || !OutputPaths[0].FullName.Contains("-armv7")))
			{
				Logger.LogInformation("================================================================================================================================");
				Logger.LogInformation("Non-default apk(s) have been made: If you are debugging, you will need to manually select one to run in the debugger properties!");
				Logger.LogInformation("================================================================================================================================");
			}
			return true;
		}

		// Store generated package name in a text file for builds that do not generate an apk file 
		public bool SavePackageInfo(string TargetName, string ProjectDirectory, TargetType InTargetType, bool bIsEmbedded)
		{
			string PackageName = GetPackageName(TargetName);
			string DestPackageNameFileName = Path.Combine(ProjectDirectory, "Binaries", "Android", "packageInfo.txt");

			string[] PackageInfoSource = new string[4];
			PackageInfoSource[0] = PackageName;
			PackageInfoSource[1] = GetStoreVersion(null).ToString();
			PackageInfoSource[2] = GetVersionDisplayName(bIsEmbedded);
			PackageInfoSource[3] = String.Format("name='com.epicgames.unreal.GameActivity.AppType' value='{0}'", InTargetType == TargetType.Game ? "" : InTargetType.ToString());

			Logger.LogInformation("Writing packageInfo pkgName:{PkgName} storeVersion:{StoreVer} versionDisplayName:{Version} to {DestFile}", PackageInfoSource[0], PackageInfoSource[1], PackageInfoSource[2], DestPackageNameFileName);

			string DestDirectory = Path.GetDirectoryName(DestPackageNameFileName)!;
			if (!Directory.Exists(DestDirectory))
			{
				Directory.CreateDirectory(DestDirectory);
			}

			File.WriteAllLines(DestPackageNameFileName, PackageInfoSource);

			return true;
		}

		public static bool ShouldMakeSeparateApks()
		{
			// @todo android fat binary: Currently, there isn't much utility in merging multiple .so's into a single .apk except for debugging,
			// but we can't properly handle multiple GPU architectures in a single .apk, so we are disabling the feature for now
			// The user will need to manually select the apk to run in their Visual Studio debugger settings (see Override APK in TADP, for instance)
			// If we change this, pay attention to <OverrideAPKPath> in AndroidProjectGenerator
			return true;

			// check to see if the project wants separate apks
			// 			ConfigCacheIni Ini = nGetConfigCacheIni("Engine");
			// 			bool bSeparateApks = false;
			// 			Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bSplitIntoSeparateApks", out bSeparateApks);
			// 
			// 			return bSeparateApks;
		}

		public bool PrepForUATPackageOrDeploy(FileReference ProjectFile, string ProjectName, DirectoryReference ProjectDirectory, string ExecutablePath, string EngineDirectory, bool bForDistribution, string CookFlavor, UnrealTargetConfiguration Configuration, bool bIsDataDeploy, bool bSkipGradleBuild, bool bIsArchive)
		{
			//Logger.LogInformation("$$$$$$$$$$$$$$ PrepForUATPackageOrDeploy $$$$$$$$$$$$$$$$$");

			TargetType Type = TargetType.Game;
			if (CookFlavor.EndsWith("Client"))
			{
				Type = TargetType.Client;
			}
			else if (CookFlavor.EndsWith("Server"))
			{
				Type = TargetType.Server;
			}

			// note that we cannot allow the data packaged into the APK if we are doing something like Launch On that will not make an obb
			// file and instead pushes files directly via deploy
			AndroidTargetRules TargetRules = new AndroidTargetRules();
			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), TargetRules, Logger);
			ClangToolChainOptions Options = AndroidPlatform.CreateToolChainOptions(TargetRules);
			AndroidToolChain ToolChain = new AndroidToolChain(ProjectFile, Options, Logger);

			// hunt down the receipt that compiled the .so
			//FileReference ReceiptFilename = TargetReceipt.GetDefaultPath(ProjectDirectory, ProjectName, SC.StageTargetPlatform.IniPlatformType, Configuration, Architecture);
			//if (!FileReference.Exists(ReceiptFilename))
			//{
			//	ReceiptFilename = TargetReceipt.GetDefaultPath(Unreal.EngineDirectory, ReceiptName, SC.StageTargetPlatform.IniPlatformType, Configuration, Architecture);
			//}
			FileReference ReceiptFilename = new FileReference(ExecutablePath).ChangeExtension(".target");
			TargetReceipt Receipt = TargetReceipt.Read(ReceiptFilename);

			SavePackageInfo(ProjectName, ProjectDirectory.FullName, Type, bSkipGradleBuild);

			MakeApk(ToolChain, ProjectName, Type, ProjectDirectory.FullName, ExecutablePath, EngineDirectory, bForDistribution: bForDistribution, CookFlavor: CookFlavor, Configuration: Configuration,
				bMakeSeparateApks: ShouldMakeSeparateApks(), bIncrementalPackage: false, bDisallowPackagingDataInApk: bIsDataDeploy, bDisallowExternalFilesDir: bIsDataDeploy,
				bSkipGradleBuild: bSkipGradleBuild, bIsArchive: bIsArchive, bIsFromUAT: true);
			return true;
		}

		public static void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line, ILogger Logger)
		{
			if ((Line != null) && (Line.Data != null))
			{
				Logger.LogInformation("{Output}", Line.Data);
			}
		}

		private string GenerateTemplatesHashCode(string EngineDir)
		{
			string SourceDirectory = Path.Combine(EngineDir, "Build", "Android", "Java");

			if (!Directory.Exists(SourceDirectory))
			{
				return "badpath";
			}

			MD5 md5 = MD5.Create();
			byte[]? TotalHashBytes = null;

			string[] SourceFiles = Directory.GetFiles(SourceDirectory, "*.*", SearchOption.AllDirectories);
			foreach (string Filename in SourceFiles)
			{
				using (FileStream stream = File.OpenRead(Filename))
				{
					byte[] FileHashBytes = md5.ComputeHash(stream);
					if (TotalHashBytes != null)
					{
						int index = 0;
						foreach (byte b in FileHashBytes)
						{
							TotalHashBytes[index] ^= b;
							index++;
						}
					}
					else
					{
						TotalHashBytes = FileHashBytes;
					}
				}
			}

			if (TotalHashBytes != null)
			{
				string HashCode = "";
				foreach (byte b in TotalHashBytes)
				{
					HashCode += b.ToString("x2");
				}
				return HashCode;
			}

			return "empty";
		}

		private void UpdateGameActivity(UnrealArch UnrealArch, string NDKArch, string EngineDir, string UnrealBuildPath, bool bDontBundleLibrariesInAPK)
		{
			string SourceFilename = Path.Combine(EngineDir, "Build", "Android", "Java", "src", "com", "epicgames", "unreal", "GameActivity.java.template");
			string DestFilename = Path.Combine(UnrealBuildPath, "src", "com", "epicgames", "unreal", "GameActivity.java");

			// check for GameActivity.java.template override
			SourceFilename = UPL!.ProcessPluginNode(NDKArch, "gameActivityReplacement", SourceFilename);

			ConfigHierarchy Ini = GetConfigCacheIni(ConfigHierarchyType.Engine);

			string GameActivityImportAdditionsDefault = "";
			string GameActivityClassAdditionsDefault = "";
			string LoadUnrealDefault = "";
			string LoadLibraryDefaults = "";

			string SuperClassDefault;
			if (!Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "GameActivitySuperClass", out SuperClassDefault))
			{
				SuperClassDefault = UPL.ProcessPluginNode(NDKArch, "gameActivitySuperClass", "");
				if (String.IsNullOrEmpty(SuperClassDefault))
				{
					SuperClassDefault = "NativeActivity";
				}
			}

			if (bDontBundleLibrariesInAPK) {
				LoadUnrealDefault = @"		System.load(GameApplication.getAppContext().getFilesDir().getAbsolutePath() + ""/libUnreal.so"");";

				GameActivityImportAdditionsDefault = @"
import dalvik.system.BaseDexClassLoader;
import java.util.Collection;
";

				GameActivityClassAdditionsDefault = @"
	@Override
	public ClassLoader getClassLoader() {
		ClassLoader baseClassLoader = super.getClassLoader();

		try {
			BaseDexClassLoader dexClassLoader = (BaseDexClassLoader) baseClassLoader;
			if (dexClassLoader.findLibrary(""Unreal"") == null) {
				Field pathListField = BaseDexClassLoader.class.getDeclaredField(""pathList"");
				pathListField.setAccessible(true);

				Object pathListObj = pathListField.get(dexClassLoader);

				Method addNativePathMethod = pathListObj.getClass().getMethod(""addNativePath"", Collection.class);

				String filesDir = getApplicationContext().getFilesDir().getAbsolutePath();
				Log.verbose(""Adding '"" + filesDir + ""' to GameActivity BaseDexClassLoader"");
				Collection<String> paths = Arrays.asList(filesDir);
				addNativePathMethod.invoke(pathListObj, paths);
			}
		} catch (Exception e) {
			Log.warn(""Failed to add native library path due to "" + e);
		}

		return baseClassLoader;
	}
";
			} else {
				LoadUnrealDefault = @"		System.loadLibrary(""Unreal"");";
			}

			string AndroidGraphicsDebugger;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "AndroidGraphicsDebugger", out AndroidGraphicsDebugger);
			switch (AndroidGraphicsDebugger.ToLower())
			{
				case "mali":
					LoadLibraryDefaults += "\t\ttry\n" +
											"\t\t{\n" +
											"\t\t\tSystem.loadLibrary(\"MGD\");\n" +
											"\t\t}\n" +
											"\t\tcatch (java.lang.UnsatisfiedLinkError e)\n" +
											"\t\t{\n" +
											"\t\t\tLog.debug(\"libMGD.so not loaded.\");\n" +
											"\t\t}\n";
					break;
			}

			Dictionary<string, string> Replacements = new Dictionary<string, string>{				
				{ "//$${gameActivityMemStatAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityMemStatAdditions", "")},
				{ "//$${gameActivityImportAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityImportAdditions", GameActivityImportAdditionsDefault)},
				{ "//$${gameActivityPostImportAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityPostImportAdditions", "")},
				{ "//$${gameActivityImplementsAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityImplementsAdditions", "")},
				{ "//$${gameActivityClassAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityClassAdditions", GameActivityClassAdditionsDefault)},
				{ "//$${gameActivityReadMetadataAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityReadMetadataAdditions", "")},
				{ "//$${gameActivityOnCreateBeginningAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnCreateBeginningAdditions", "")},
				{ "//$${gameActivityOnCreateAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnCreateAdditions", "")},
				{ "//$${gameActivityOnCreateFinalAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnCreateFinalAdditions", "")},
				{ "//$${gameActivityOverrideAPKOBBPackaging}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOverrideAPKOBBPackaging", "")},
				{ "//$${gameActivityOnDestroyAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnDestroyAdditions", "")},
				{ "//$${gameActivityonConfigurationChangedAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityonConfigurationChangedAdditions", "")},
				{ "//$${gameActivityOnStartAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnStartAdditions", "")},
				{ "//$${gameActivityOnStopAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnStopAdditions", "")},
				{ "//$${gameActivityOnRestartAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnRestartAdditions", "")},
				{ "//$${gameActivityOnSaveInstanceStateAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnSaveInstanceStateAdditions", "")},
				{ "//$${gameActivityOnRequestPermissionsResultAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnRequestPermissionsResultAdditions", "")},
				{ "//$${gameActivityOnPauseAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnPauseAdditions", "")},
				{ "//$${gameActivityOnResumeAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnResumeAdditions", "")},
				{ "//$${gameActivityOnNewIntentAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnNewIntentAdditions", "")},
  				{ "//$${gameActivityOnActivityResultAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnActivityResultAdditions", "")},
  				{ "//$${gameActivityPreConfigRulesParseAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityPreConfigRulesParseAdditions", "")},
  				{ "//$${gameActivityPostConfigRulesAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityPostConfigRulesAdditions", "")},
  				{ "//$${gameActivityFinalizeConfigRulesAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityFinalizeConfigRulesAdditions", "")},
				{ "//$${gameActivityBeforeConfigRulesAppliedAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityBeforeConfigRulesAppliedAdditions", "")},
				{ "//$${gameActivityAfterMainViewCreatedAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityAfterMainViewCreatedAdditions", "")},
				{ "//$${gameActivityResizeKeyboardAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityResizeKeyboardAdditions", "")},
				{ "//$${gameActivityLoggerCallbackAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityLoggerCallbackAdditions", "")},
				{ "//$${gameActivityGetCommandLineAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityGetCommandLineAdditions", "")},
				{ "//$${gameActivityGetLoginIdAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityGetLoginIdAdditions", "")},
				{ "//$${gameActivityGetFunnelIdAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityGetFunnelIdAdditions", "")},
				{ "//$${gameActivityAllowedRemoteNotificationsAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityAllowedRemoteNotificationsAdditions", "")},
				{ "//$${gameActivityAndroidThunkJavaIapBeginPurchase}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityAndroidThunkJavaIapBeginPurchase", "")},
				{ "//$${gameActivityIapSetupServiceAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityIapSetupServiceAdditions", "")},
				{ "//$${gameActivityOnRestartApplicationAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityOnRestartApplicationAdditions", "")},
				{ "//$${gameActivityForceQuitAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameActivityForceQuitAdditions", "")},
				{ "//$${gameActivitySetCrashContextData}$$", UPL.ProcessPluginNode(NDKArch, "gameActivitySetCrashContextData", "")},
				{ "//$${soLoadUnreal}$$", UPL.ProcessPluginNode(NDKArch, "soLoadUnreal", LoadUnrealDefault)},
				{ "//$${soLoadLibrary}$$", UPL.ProcessPluginNode(NDKArch, "soLoadLibrary", LoadLibraryDefaults)},
				{ "$${gameActivitySuperClass}$$", SuperClassDefault},
			};

			string[] TemplateSrc = File.ReadAllLines(SourceFilename);
			string[]? TemplateDest = File.Exists(DestFilename) ? File.ReadAllLines(DestFilename) : null;

			bool TemplateChanged = false;
			for (int LineIndex = 0; LineIndex < TemplateSrc.Length; ++LineIndex)
			{
				string SrcLine = TemplateSrc[LineIndex];
				bool Changed = false;
				foreach (KeyValuePair<string, string> KVP in Replacements)
				{
					if (SrcLine.Contains(KVP.Key))
					{
						SrcLine = SrcLine.Replace(KVP.Key, KVP.Value);
						Changed = true;
					}
				}
				if (Changed)
				{
					TemplateSrc[LineIndex] = SrcLine;
					TemplateChanged = true;
				}
			}

			if (TemplateChanged)
			{
				// deal with insertions of newlines
				TemplateSrc = String.Join("\n", TemplateSrc).Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);
			}

			if (TemplateDest == null || TemplateSrc.Length != TemplateDest.Length || !TemplateSrc.SequenceEqual(TemplateDest))
			{
				Logger.LogInformation("");
				Logger.LogInformation("==== Writing new GameActivity.java file to {DestFile} ====", DestFilename);
				File.WriteAllLines(DestFilename, TemplateSrc);
			}
		}

		private void UpdateGameApplication(UnrealArch UnrealArch, string NDKArch, string EngineDir, string UnrealBuildPath)
		{
			string SourceFilename = Path.Combine(EngineDir, "Build", "Android", "Java", "src", "com", "epicgames", "unreal", "GameApplication.java.template");
			string DestFilename = Path.Combine(UnrealBuildPath, "src", "com", "epicgames", "unreal", "GameApplication.java");

			Dictionary<string, string> Replacements = new Dictionary<string, string>{
				{ "//$${gameApplicationImportAdditions}$$", UPL!.ProcessPluginNode(NDKArch, "gameApplicationImportAdditions", "")},
				{ "//$${gameApplicationOnCreateAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameApplicationOnCreateAdditions", "")},
				{ "//$${gameApplicationAttachBaseContextAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameApplicationAttachBaseContextAdditions", "")},
				{ "//$${gameApplicationOnLowMemoryAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameApplicationOnLowMemoryAdditions", "")},
				{ "//$${gameApplicationOnTrimMemoryAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameApplicationOnTrimMemoryAdditions", "")},
				{ "//$${gameApplicationOnConfigurationChangedAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameApplicationOnConfigurationChangedAdditions", "")},
				{ "//$${gameApplicationClassAdditions}$$", UPL.ProcessPluginNode(NDKArch, "gameApplicationClassAdditions", "")},
			};

			string[] TemplateSrc = File.ReadAllLines(SourceFilename);
			string[]? TemplateDest = File.Exists(DestFilename) ? File.ReadAllLines(DestFilename) : null;

			bool TemplateChanged = false;
			for (int LineIndex = 0; LineIndex < TemplateSrc.Length; ++LineIndex)
			{
				string SrcLine = TemplateSrc[LineIndex];
				bool Changed = false;
				foreach (KeyValuePair<string, string> KVP in Replacements)
				{
					if (SrcLine.Contains(KVP.Key))
					{
						SrcLine = SrcLine.Replace(KVP.Key, KVP.Value);
						Changed = true;
					}
				}
				if (Changed)
				{
					TemplateSrc[LineIndex] = SrcLine;
					TemplateChanged = true;
				}
			}

			if (TemplateChanged)
			{
				// deal with insertions of newlines
				TemplateSrc = String.Join("\n", TemplateSrc).Split(new[] { "\r\n", "\r", "\n" }, StringSplitOptions.None);
			}

			if (TemplateDest == null || TemplateSrc.Length != TemplateDest.Length || !TemplateSrc.SequenceEqual(TemplateDest))
			{
				Logger.LogInformation("");
				Logger.LogInformation("==== Writing new GameApplication.java file to {DestFile} ====", DestFilename);
				File.WriteAllLines(DestFilename, TemplateSrc);
			}
		}

		private void CreateAdditonalBuildPathFiles(string NDKArch, string UnrealBuildPath, XDocument FilesToAdd)
		{
			Dictionary<string, string?> PathsAndRootEls = new Dictionary<string, string?>();

			foreach (XElement Element in FilesToAdd.Root!.Elements())
			{
				string RelPath = Element.Value;
				if (RelPath != null)
				{
					XAttribute? TypeAttr = Element.Attribute("rootEl");
					PathsAndRootEls[RelPath] = TypeAttr?.Value;
				}
			}

			foreach (KeyValuePair<string, string?> Entry in PathsAndRootEls)
			{
				string UPLNodeName = Entry.Key.Replace("/", "__").Replace(".", "__");
				string Content;
				if (Entry.Value == null)
				{
					// no root element, assume not XML
					Content = UPL!.ProcessPluginNode(NDKArch, UPLNodeName, "");
				}
				else
				{
					XDocument ContentDoc = new XDocument(new XElement(Entry.Value));
					UPL!.ProcessPluginNode(NDKArch, UPLNodeName, "", ref ContentDoc);
					Content = XML_HEADER + "\n" + ContentDoc.ToString();
				}

				string DestPath = Path.Combine(UnrealBuildPath, Entry.Key);
				if (!File.Exists(DestPath) || File.ReadAllText(DestPath) != Content)
				{
					File.WriteAllText(DestPath, Content);
				}
			}
		}

		private AndroidAARHandler CreateAARHandler(string EngineDir, string UnrealBuildPath, List<string> NDKArches, bool HandleDependencies = true)
		{
			AndroidAARHandler AARHandler = new AndroidAARHandler();
			string ImportList = "";

			// Get some common paths
			string AndroidHome = Environment.ExpandEnvironmentVariables("%ANDROID_HOME%").TrimEnd('/', '\\');
			EngineDir = EngineDir.TrimEnd('/', '\\');

			// Add the AARs from the default aar-imports.txt
			// format: Package,Name,Version
			string ImportsFile = Path.Combine(UnrealBuildPath, "aar-imports.txt");
			if (File.Exists(ImportsFile))
			{
				ImportList = File.ReadAllText(ImportsFile);
			}

			// Run the UPL imports section for each architecture and add any new imports (duplicates will be removed)
			foreach (string NDKArch in NDKArches)
			{
				ImportList = UPL!.ProcessPluginNode(NDKArch, "AARImports", ImportList);
			}

			// Add the final list of imports and get dependencies
			foreach (string Line in ImportList.Split('\n'))
			{
				string Trimmed = Line.Trim(' ', '\r');

				if (Trimmed.StartsWith("repository "))
				{
					string DirectoryPath = Trimmed.Substring(11).Trim(' ').TrimEnd('/', '\\');
					DirectoryPath = DirectoryPath.Replace("$(ENGINEDIR)", EngineDir);
					DirectoryPath = DirectoryPath.Replace("$(ANDROID_HOME)", AndroidHome);
					DirectoryPath = DirectoryPath.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
					AARHandler.AddRepository(DirectoryPath, Logger);
				}
				else if (Trimmed.StartsWith("repositories "))
				{
					string DirectoryPath = Trimmed.Substring(13).Trim(' ').TrimEnd('/', '\\');
					DirectoryPath = DirectoryPath.Replace("$(ENGINEDIR)", EngineDir);
					DirectoryPath = DirectoryPath.Replace("$(ANDROID_HOME)", AndroidHome);
					DirectoryPath = DirectoryPath.Replace('\\', Path.DirectorySeparatorChar).Replace('/', Path.DirectorySeparatorChar);
					AARHandler.AddRepositories(DirectoryPath, "m2repository", Logger);
				}
				else
				{
					string[] Sections = Trimmed.Split(',');
					if (Sections.Length == 3)
					{
						string PackageName = Sections[0].Trim(' ');
						string BaseName = Sections[1].Trim(' ');
						string Version = Sections[2].Trim(' ');
						Logger.LogInformation("AARImports: {PackageName}, {BaseName}, {Version}", PackageName, BaseName, Version);
						AARHandler.AddNewAAR(PackageName, BaseName, Version, Logger, HandleDependencies);
					}
				}
			}

			return AARHandler;
		}

		private void PrepareJavaLibsForGradle(string JavaLibsDir, string UnrealBuildGradlePath, string InMinSdkVersion, string InTargetSdkVersion, string CompileSDKVersion, string BuildToolsVersion, string NDKArch)
		{
			StringBuilder SettingsGradleContent = new StringBuilder();
			StringBuilder ProjectDependencyContent = new StringBuilder();

			SettingsGradleContent.AppendLine("rootProject.name='app'");
			SettingsGradleContent.AppendLine("include ':app'");
			ProjectDependencyContent.AppendLine("dependencies {");

			string[] LibDirs = Directory.GetDirectories(JavaLibsDir);
			foreach (string LibDir in LibDirs)
			{
				string RelativePath = Path.GetFileName(LibDir);

				SettingsGradleContent.AppendLine(String.Format("include ':{0}'", RelativePath));
				ProjectDependencyContent.AppendLine(String.Format("\timplementation project(':{0}')", RelativePath));

				string GradleProjectPath = Path.Combine(UnrealBuildGradlePath, RelativePath);
				string GradleProjectMainPath = Path.Combine(GradleProjectPath, "src", "main");

				string ManifestFilename = Path.Combine(LibDir, "AndroidManifest.xml");
				string GradleManifest = Path.Combine(GradleProjectMainPath, "AndroidManifest.xml");
				MakeDirectoryIfRequired(GradleManifest);

				// Copy parts were they need to be
				CleanCopyDirectory(Path.Combine(LibDir, "assets"), Path.Combine(GradleProjectPath, "assets"));
				CleanCopyDirectory(Path.Combine(LibDir, "libs"), Path.Combine(GradleProjectPath, "libs"));
				CleanCopyDirectory(Path.Combine(LibDir, "res"), Path.Combine(GradleProjectMainPath, "res"));

				// If our lib already has a src/main/java folder, don't put things into a java folder
				string SrcDirectory = Path.Combine(LibDir, "src", "main");
				if (Directory.Exists(Path.Combine(SrcDirectory, "java")))
				{
					CleanCopyDirectory(SrcDirectory, GradleProjectMainPath);
				}
				else
				{
					CleanCopyDirectory(Path.Combine(LibDir, "src"), Path.Combine(GradleProjectMainPath, "java"));
				}

				// Now generate a build.gradle from the manifest
				StringBuilder BuildGradleContent = new StringBuilder();
				BuildGradleContent.AppendLine("apply plugin: 'com.android.library'");
				BuildGradleContent.AppendLine("android {");
				BuildGradleContent.AppendLine("\tndkPath = System.getenv(\"NDKROOT\")");
				BuildGradleContent.AppendLine("\tcompileSdkVersion = COMPILE_SDK_VERSION.toInteger()");
				BuildGradleContent.AppendLine("\tbuildToolsVersion = BUILD_TOOLS_VERSION");
				BuildGradleContent.AppendLine("\tdefaultConfig {");

				// Try to get the SDK target from the AndroidManifest.xml
				string VersionCode = "";
				string VersionName = "";
				string MinSdkVersion = InMinSdkVersion;
				string TargetSdkVersion = InTargetSdkVersion;
				XDocument ManifestXML;
				if (File.Exists(ManifestFilename))
				{
					try
					{
						ManifestXML = XDocument.Load(ManifestFilename);

						XAttribute? VersionCodeAttr = ManifestXML.Root!.Attribute(XName.Get("versionCode", "http://schemas.android.com/apk/res/android"));
						if (VersionCodeAttr != null)
						{
							VersionCode = VersionCodeAttr.Value;
						}

						XAttribute? VersionNameAttr = ManifestXML.Root.Attribute(XName.Get("versionName", "http://schemas.android.com/apk/res/android"));
						if (VersionNameAttr != null)
						{
							VersionName = VersionNameAttr.Value;
						}

						XElement? UseSDKNode = null;
						foreach (XElement WorkNode in ManifestXML.Elements().First().Descendants("uses-sdk"))
						{
							UseSDKNode = WorkNode;

							XAttribute? MinSdkVersionAttr = WorkNode.Attribute(XName.Get("minSdkVersion", "http://schemas.android.com/apk/res/android"));
							if (MinSdkVersionAttr != null)
							{
								MinSdkVersion = MinSdkVersionAttr.Value;
							}

							XAttribute? TargetSdkVersionAttr = WorkNode.Attribute(XName.Get("targetSdkVersion", "http://schemas.android.com/apk/res/android"));
							if (TargetSdkVersionAttr != null)
							{
								TargetSdkVersion = TargetSdkVersionAttr.Value;
							}
						}

						if (UseSDKNode != null)
						{
							UseSDKNode.Remove();
						}

						// rewrite the manifest if different
						String NewManifestText = ManifestXML.ToString();
						String OldManifestText = "";
						if (File.Exists(GradleManifest))
						{
							OldManifestText = File.ReadAllText(GradleManifest);
						}
						if (NewManifestText != OldManifestText)
						{
							File.WriteAllText(GradleManifest, NewManifestText);
						}
					}
					catch (Exception e)
					{
						Logger.LogError(e, "AAR Manifest file {FileName} parsing error! {Ex}", ManifestFilename, e);
					}
				}

				if (!String.IsNullOrEmpty(VersionCode))
				{
					BuildGradleContent.AppendLine(String.Format("\t\tversionCode {0}", VersionCode));
				}
				if (!String.IsNullOrEmpty(VersionName))
				{
					BuildGradleContent.AppendLine(String.Format("\t\tversionName \"{0}\"", VersionName));
				}
				if (!String.IsNullOrEmpty(MinSdkVersion))
				{
					BuildGradleContent.AppendLine(String.Format("\t\tminSdkVersion = {0}", MinSdkVersion));
				}
				if (!String.IsNullOrEmpty(TargetSdkVersion))
				{
					BuildGradleContent.AppendLine(String.Format("\t\ttargetSdkVersion = {0}", TargetSdkVersion));
				}
				BuildGradleContent.AppendLine("\t}");
				BuildGradleContent.AppendLine("}");

				string AdditionsGradleFilename = Path.Combine(LibDir, "additions.gradle");
				if (File.Exists(AdditionsGradleFilename))
				{
					string[] AdditionsLines = File.ReadAllLines(AdditionsGradleFilename);
					foreach (string LineContents in AdditionsLines)
					{
						BuildGradleContent.AppendLine(LineContents);
					}
				}

				// rewrite the build.gradle if different
				string BuildGradleFilename = Path.Combine(GradleProjectPath, "build.gradle");
				String NewBuildGradleText = BuildGradleContent.ToString();
				String OldBuildGradleText = "";
				if (File.Exists(BuildGradleFilename))
				{
					OldBuildGradleText = File.ReadAllText(BuildGradleFilename);
				}
				if (NewBuildGradleText != OldBuildGradleText)
				{
					File.WriteAllText(BuildGradleFilename, NewBuildGradleText);
				}
			}
			ProjectDependencyContent.AppendLine("}");

			// Add any UPL settingsGradleAdditions
			SettingsGradleContent.Append(UPL!.ProcessPluginNode(NDKArch, "settingsGradleAdditions", ""));

			string SettingsGradleFilename = Path.Combine(UnrealBuildGradlePath, "settings.gradle");
			File.WriteAllText(SettingsGradleFilename, SettingsGradleContent.ToString());

			string ProjectsGradleFilename = Path.Combine(UnrealBuildGradlePath, "app", "projects.gradle");
			File.WriteAllText(ProjectsGradleFilename, ProjectDependencyContent.ToString());
		}

		private void GenerateGradleAARImports(string EngineDir, string UnrealBuildPath, List<string> NDKArches)
		{
			AndroidAARHandler AARHandler = CreateAARHandler(EngineDir, UnrealBuildPath, NDKArches, false);
			StringBuilder AARImportsContent = new StringBuilder();

			// Add repositories
			AARImportsContent.AppendLine("repositories {");
			foreach (string Repository in AARHandler.Repositories!)
			{
				string RepositoryPath = Path.GetFullPath(Repository).Replace('\\', '/');
				AARImportsContent.AppendLine("\tmaven { url uri('" + RepositoryPath + "') }");
			}
			AARImportsContent.AppendLine("}");

			// Add dependencies
			AARImportsContent.AppendLine("dependencies {");
			foreach (AndroidAARHandler.AndroidAAREntry Dependency in AARHandler.AARList!)
			{
				AARImportsContent.AppendLine(String.Format("\timplementation '{0}:{1}:{2}'", Dependency.Filename, Dependency.BaseName, Dependency.Version));
			}
			AARImportsContent.AppendLine("}");

			string AARImportsFilename = Path.Combine(UnrealBuildPath, "gradle", "app", "aar-imports.gradle");
			File.WriteAllText(AARImportsFilename, AARImportsContent.ToString());
		}
	}
}
