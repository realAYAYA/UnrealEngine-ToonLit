// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnrealBuildTool;
using EpicGames.Core;

public class EOSSDK : ModuleRules
{
	public virtual bool bHasPlatformBaseFile { get { return false; } }
	public virtual bool bHasMultiplePlatformSDKBuilds { get { return false; } }

	public virtual string EOSSDKPlatformName
	{
		get
		{
			return Target.Platform.ToString();
		}
	}

	public virtual string EOSSDKIdealPlatformSDKBuildName
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
			if (Target.Platform == UnrealTargetPlatform.Android)
			{
				return Path.Combine(SDKBinariesDir, "include");
			}
			else if(Target.Platform == UnrealTargetPlatform.IOS)
            {
				return Path.Combine(SDKBinariesDir, "EOSSDK.framework", "Headers");
			}
			else
			{
				return Path.Combine(SDKBaseDir, "Include");
			}
		}
	}

	public string SDKLibsDir
	{
		get
		{
			string LibDir = Path.Combine(SDKBaseDir, "Lib");
			string PlatformLibDir = Path.Combine(LibDir, EOSSDKPlatformName);
			if (Directory.Exists(PlatformLibDir))
			{
				return PlatformLibDir;
			}
			return LibDir;
		}
	}

	public string SDKBinariesDir
	{
		get
		{
			string BinDir = SDKBaseDir;
			if (bHasMultiplePlatformSDKBuilds)
			{
				string PlatformSDKBuildDir = Path.Combine(BinDir, EOSSDKPlatformName + "-" + EOSSDKIdealPlatformSDKBuildName);
				if(!Directory.Exists(PlatformSDKBuildDir))
				{
					// Fall back on any available one.
					PlatformSDKBuildDir = Directory.GetDirectories(SDKBaseDir, EOSSDKPlatformName + "-*").First();
					string Fallback = PlatformSDKBuildDir.Split(EOSSDKPlatformName + "-").Last();
					Log.TraceWarningOnce("Unable to find EOSSDK for platform SDK \"{0}\", falling back on EOSSDK for platform SDK \"{1}\".", EOSSDKIdealPlatformSDKBuildName, Fallback);
				}
				BinDir = PlatformSDKBuildDir;
			}
			BinDir = Path.Combine(BinDir, "Bin");
			string PlatformBinDir = Path.Combine(BinDir, EOSSDKPlatformName);
			if (Directory.Exists(PlatformBinDir))
			{
				BinDir = PlatformBinDir;
			}
			return BinDir;
		}
	}

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

	public EOSSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		bool bIsMonolithic = Target.LinkType == TargetLinkType.Monolithic;
		bool bIsUniqueBuildEnv = Target.BuildEnvironment == TargetBuildEnvironment.Unique;
		bool bUseProjectBinary = (bIsMonolithic || bIsUniqueBuildEnv) && Target.GlobalDefinitions.Contains("EOSSDK_USE_PROJECT_BINARY=1");

		if (!bUseProjectBinary && Target.Platform == UnrealTargetPlatform.LinuxArm64)
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

			if (!bUseProjectBinary)
			{
				PublicAdditionalLibraries.Add(Path.Combine(SDKBinariesDir, "static-stdc++", "libs", "armeabi-v7a", RuntimeLibraryFileName));
				PublicAdditionalLibraries.Add(Path.Combine(SDKBinariesDir, "static-stdc++", "libs", "arm64-v8a", RuntimeLibraryFileName));

				string PluginPath = Utils.MakePathRelativeTo(ModuleDirectory, Target.RelativeEnginePath);
				AdditionalPropertiesForReceipt.Add("AndroidPlugin", Path.Combine(PluginPath, "EOSSDK_UPL.xml"));
			}
        }
		else if (Target.Platform == UnrealTargetPlatform.IOS)
		{
			if (!bUseProjectBinary)
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
			// Allow projects to provide their own EOSSDK binaries. We will still compile against our own headers, because EOSSDK makes guarantees about forward compat. Note this global definition is only valid for monolithic targets.
			if(!bUseProjectBinary)
            {
				PublicAdditionalLibraries.Add(Path.Combine(SDKBinariesDir, LibraryLinkName));
				RuntimeDependencies.Add(Path.Combine(EngineBinariesDir, RuntimeLibraryFileName), Path.Combine(SDKBinariesDir, RuntimeLibraryFileName));

				// needed for linux to find the .so
				PublicRuntimeLibraryPaths.Add(EngineBinariesDir);
				
				if (bRequiresRuntimeLoad)
				{
					PublicDelayLoadDLLs.Add(RuntimeLibraryFileName);
				}
			}
		}
	}
}
