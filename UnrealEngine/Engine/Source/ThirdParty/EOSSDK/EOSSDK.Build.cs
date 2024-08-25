// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnrealBuildTool;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

public class EOSSDK : ModuleRules
{
	public virtual bool bHasPlatformBaseFile { get { return false; } }
	public virtual bool bHasMultiplePlatformSDKBuilds { get { return false; } }
	public string KittTargetConfiguration { get { return "shipping"; } }

	public virtual string EOSSDKPlatformName
	{
		get
		{
			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows))
			{
				return "Win64";
			}
			else
			{
				return Target.Platform.ToString();
			}
		}
	}

	public virtual string EOSSDKIdealPlatformSDKVersion
	{
		get
		{
			return UEBuildPlatformSDK.GetSDKForPlatform(Target.Platform.ToString()).GetInstalledVersion();
		}
	}

	public virtual string SDKBaseDir
	{
		get
		{
			// Prefer to use the SDK dir from the platform extension.
			// Will fail if this is not a platform extension, or there is no SDK in the platform extension.
			string PlatformExtensionBaseDir = GetModuleDirectoryForSubClass(this.GetType())?.FullName;
			if(PlatformExtensionBaseDir != null)
			{
				PlatformExtensionBaseDir = Path.Combine(PlatformExtensionBaseDir, "SDK");
				if(Directory.Exists(PlatformExtensionBaseDir))
				{
					return PlatformExtensionBaseDir;
				}
			}
			// Fallback on the base module SDK.
			return Path.Combine(ModuleDirectory, "SDK");
		}
	}

	private string SDKIncludesDir
	{
		get
		{
			if(Target.Platform == UnrealTargetPlatform.IOS)
            {
				return Path.Combine(SDKBinariesDir, "EOSSDK.framework", "Headers");
			}
			else
			{
				return Path.Combine(SDKBaseDir, "Include");
			}
		}
	}

	private string FindPlatformSubDir(string ParentDir)
	{
		string PlatformDir = Path.Combine(ParentDir, EOSSDKPlatformName);
		if (bHasMultiplePlatformSDKBuilds)
		{
			List<string> AvailableBuilds = new List<string>(Directory.GetDirectories(PlatformDir));
			string IdealBuildDir = AvailableBuilds.FirstOrDefault(Elem => Elem.Split("\\").Last().StartsWith(EOSSDKIdealPlatformSDKVersion));
			if (Directory.Exists(IdealBuildDir))
			{
				Log.TraceVerboseOnce("Found EOSSDK for platform SDK \"{0}\" at \"{1}\".", EOSSDKIdealPlatformSDKVersion, IdealBuildDir);
				return IdealBuildDir;
			}
			else
			{
				// Sort versions in ascendent order as the array returned by Directory.GetDirectories is not properly sorted
				Comparison<string> AscendingOrder = (x, y) => int.Parse(x.Split("\\").Last().Split(".").First()).CompareTo(int.Parse(y.Split("\\").Last().Split(".").First()));
				AvailableBuilds.Sort(AscendingOrder);

				// Fall back to latest one available.
				string LatestBuildDir = AvailableBuilds.Last();
				string LatestBuildSDKVersion = LatestBuildDir.Split("\\").Last();
				Log.TraceWarningOnce("Unable to find EOSSDK for platform SDK \"{0}\", falling back on EOSSDK for platform SDK \"{1}\".", EOSSDKIdealPlatformSDKVersion, LatestBuildSDKVersion);

				return LatestBuildDir;
			}
		}
		else if (Directory.Exists(PlatformDir))
		{
			return PlatformDir;
		}
		return ParentDir;
	}

	public string SDKLibsDir => FindPlatformSubDir(Path.Combine(SDKBaseDir, "Lib"));

	public string SDKBinariesDir => FindPlatformSubDir(Path.Combine(SDKBaseDir, "Bin"));

	public virtual string EngineBinariesDir
	{
		get
		{
			return Path.Combine(EngineDirectory, "Binaries", Target.Platform.ToString());
		}
	}

	public virtual string LibraryLinkNameBase
	{
		get
		{
			if(Target.Platform == UnrealTargetPlatform.Android)
            {
				return "EOSSDK";
            }
			
			return String.Format("EOSSDK-{0}-Shipping", EOSSDKPlatformName);
		}
	}

	public virtual string LibraryLinkName
	{
		get
		{
			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				return Path.Combine(SDKBinariesDir, "lib" + LibraryLinkNameBase + ".dylib");
			}
			else if(Target.Platform == UnrealTargetPlatform.Linux ||
				Target.Platform == UnrealTargetPlatform.LinuxArm64)
			{
				return Path.Combine(SDKBinariesDir, "lib" + LibraryLinkNameBase + ".so");
			}
			else if (Target.Platform.IsInGroup(UnrealPlatformGroup.Microsoft))
			{
				return Path.Combine(SDKLibsDir, LibraryLinkNameBase + ".lib");
			}
			// Android has one .so per architecture, so just deal with that below.
			// Other platforms will override this property.

			throw new BuildException("Unsupported platform");
		}
	}

	public virtual string RuntimeLibraryFileName
	{
		get
		{
			if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				return "lib" + LibraryLinkNameBase + ".dylib";
			}
			else if (Target.Platform == UnrealTargetPlatform.IOS)
			{
				return LibraryLinkNameBase + ".framework";
			}
			else if (Target.Platform == UnrealTargetPlatform.Android ||
				Target.Platform == UnrealTargetPlatform.Linux ||
				Target.Platform == UnrealTargetPlatform.LinuxArm64)
			{
				return "lib" + LibraryLinkNameBase + ".so";
			}
			else if (Target.Platform.IsInGroup(UnrealPlatformGroup.Microsoft))
			{
				return LibraryLinkNameBase + ".dll";
			}
			// Other platforms will override this property.

			throw new BuildException("Unsupported platform");
		}
	}

	public virtual bool bRequiresRuntimeLoad
	{
		get
		{
			return Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) || Target.Platform == UnrealTargetPlatform.Mac;
			// Other platforms may override this property.
		}
	}

	/**
	 * Allow projects to provide their own EOSSDK binaries.
	 * In monolithic targets, prevents this module adding EOSSDK binaries to the linker args. The project is expected to add EOSSDK binaries to the linker args instead.
	 * In unique build environments, prevents this module staging EOSSDK binaries. The project is expected to stage EOSSDK binaries instead.
	 */
	[ConfigFile(ConfigHierarchyType.Engine, "EOSSDK")]
	bool bHasProjectBinary = false;

	bool HasProjectBinary
	{
		get
		{
			// Keep supporting old GlobalDef mechanism in case licensees are using it
			if (Target.GlobalDefinitions.Contains("EOSSDK_USE_PROJECT_BINARY=1"))
			{
				Logger.LogWarning("EOSSDK_USE_PROJECT_BINARY is deprecated, please use Engine.ini [EOSSDK] bHasProjectBinary=true config mechanism instead");
				return true;
			}

			ConfigCache.ReadSettings(DirectoryReference.FromFile(Target.ProjectFile), Target.Platform, this);
			return bHasProjectBinary;
		}
	}
	
	public virtual bool bIncludeKITT { get { return false; } }

	public virtual string KittModuleName(string BaseName, string ConfigName)
	{
		return String.Format("{0}-{1}-{2}", BaseName, EOSSDKPlatformName, KittTargetConfiguration);
	}

	public virtual string KittModuleExtension()
	{
		throw new Exception("KITT is not supported on this platform: " + Target.Platform.ToString());
	}

	public virtual void AddKITTRuntimeDependency(string BaseName, string ConfigName, string SubDir = null)
	{
		// TODO: Pending further testing in other platforms. Not used by Switch
		
		string CurrentKittModuleName = KittModuleName(BaseName, ConfigName);

		string KittModuleSource = (SubDir != null && SubDir.Length > 0)
			? Path.Combine(SDKBinariesDir, SubDir, CurrentKittModuleName + KittModuleExtension())
			: Path.Combine(SDKBinariesDir, CurrentKittModuleName + KittModuleExtension());

		string KittModuleTarget = Path.Combine(EngineBinariesDir, CurrentKittModuleName + KittModuleExtension());

		RuntimeDependencies.Add(KittModuleTarget, KittModuleSource, StagedFileType.NonUFS);
	}

	public EOSSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bIsMonolithic = Target.LinkType == TargetLinkType.Monolithic;
		bool bIsUniqueBuildEnv = Target.BuildEnvironment == TargetBuildEnvironment.Unique;

		// Don't link against the SDK if this is a monolithic build and a project binary is being provided.
		bool bEnableLink = !(bIsMonolithic && HasProjectBinary);

		// Don't stage SDK binaries if we're not linking against the SDK, or if this is a unique build environment and a project binary is being provided
		bool bEnableStage = bEnableLink && !(bIsUniqueBuildEnv && HasProjectBinary);

		if (bEnableLink && Target.Platform == UnrealTargetPlatform.LinuxArm64)
        {
			// Not supported yet for non-project binaries.
			PublicDefinitions.Add("WITH_EOS_SDK=0");
			return;
        }

		PublicDefinitions.Add("WITH_EOS_SDK=1");
		PublicSystemIncludePaths.Add(SDKIncludesDir);

		PublicDefinitions.Add(String.Format("EOSSDK_RUNTIME_LOAD_REQUIRED={0}", bRequiresRuntimeLoad ? 1 : 0));
		PublicDefinitions.Add(String.Format("EOSSDK_RUNTIME_LIBRARY_NAME=\"{0}\"", RuntimeLibraryFileName));

		if(bHasPlatformBaseFile)
		{
			PublicSystemIncludePaths.Add(Path.Combine(SDKIncludesDir, EOSSDKPlatformName));
			PublicDefinitions.Add(string.Format("EOS_PLATFORM_BASE_FILE_NAME=\"eos_{0}_base.h\"", EOSSDKPlatformName));
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PublicSystemIncludePaths.Add(Path.Combine(SDKIncludesDir, "Android"));

			if (bEnableLink)
			{
				PublicAdditionalLibraries.Add(Path.Combine(SDKBinariesDir, "static-stdc++", "libs", "armeabi-v7a", RuntimeLibraryFileName));
				PublicAdditionalLibraries.Add(Path.Combine(SDKBinariesDir, "static-stdc++", "libs", "arm64-v8a", RuntimeLibraryFileName));
				PublicAdditionalLibraries.Add(Path.Combine(SDKBinariesDir, "static-stdc++", "libs", "x86_64", RuntimeLibraryFileName));

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "EOSSDK_UPL.xml"));
			}
        }
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			if (bEnableLink)
			{
				PublicAdditionalFrameworks.Add(new Framework("EOSSDK", SDKBinariesDir, "", true));
			}
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			string DylibPath = Path.Combine(Target.UEThirdPartyBinariesDirectory, "EOSSDK", "Mac", RuntimeLibraryFileName);
			PublicDelayLoadDLLs.Add(DylibPath);
			RuntimeDependencies.Add(DylibPath);
		}
		else
		{
			if(bEnableLink)
            {
				PublicAdditionalLibraries.Add(Path.Combine(SDKBinariesDir, LibraryLinkName));

				if(bEnableStage)
				{
					RuntimeDependencies.Add(Path.Combine(EngineBinariesDir, RuntimeLibraryFileName), Path.Combine(SDKBinariesDir, RuntimeLibraryFileName));
				}

				// needed for linux to find the .so
				PublicRuntimeLibraryPaths.Add(EngineBinariesDir);
				
				if (bRequiresRuntimeLoad)
				{
					PublicDelayLoadDLLs.Add(RuntimeLibraryFileName);
				}
			}
		}

		if (bIncludeKITT && bEnableStage)
		{
			AddKITTRuntimeDependency("kitt", KittTargetConfiguration);
			AddKITTRuntimeDependency("kitt_support", KittTargetConfiguration);
			AddKITTRuntimeDependency("kitt_webkit", KittTargetConfiguration);
			AddKITTRuntimeDependency("kitt_data_eos", KittTargetConfiguration);
		}
	}
}
