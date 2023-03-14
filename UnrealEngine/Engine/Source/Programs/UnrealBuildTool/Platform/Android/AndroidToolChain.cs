// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	class AndroidToolChain : ClangToolChain, IAndroidToolChain
	{
		// Minimum NDK API level to allow
		public const int MinimumNDKAPILevel = 26;

		public static readonly string[] AllCpuSuffixes =
		{
			"-arm64",
			"-x64"
		};

		// sh0rt names for the above suffixes
		public static readonly Dictionary<string, string> ShortArchNames = new Dictionary<string, string>()
		{
			{ "", "" },
			{ "-arm64", "a8" },
			{ "-x64", "x6" },
		};

		public enum ClangSanitizer
		{
			None,
			Address,
			HwAddress,
			UndefinedBehavior,
			UndefinedBehaviorMinimal,
			Thread,
		};

		public static string GetCompilerOption(ClangSanitizer Sanitizer)
		{
			switch (Sanitizer)
			{
				case ClangSanitizer.Address: return "address";
				case ClangSanitizer.HwAddress: return "hwaddress";
				case ClangSanitizer.UndefinedBehavior:
				case ClangSanitizer.UndefinedBehaviorMinimal: return "undefined";
				case ClangSanitizer.Thread: return "thread";
				default: return "";
			}
		}

		protected FileReference? ProjectFile;
		private List<string> AdditionalArches;
		private List<string> AdditionalGPUArches;
		protected bool bExecuteCompilerThroughShell;

		// Version string from the Android specific build of clang. E.g in Android (6317467 based on r365631c1) clang version 9.0.8
		// this would be 6317467)
		protected static string? AndroidClangBuild;

		// the list of architectures we will compile for
		protected List<string>? Arches = null;

		// the "-android" suffix paths here are vcpkg triplets for the android platform
		static private Dictionary<string, string[]> AllArchNames = new Dictionary<string, string[]> {
			{ "-arm64", new string[] { "arm64", "arm64-v8a", "arm64-android" } },
			{ "-x64",   new string[] { "x64", "x86_64", "x64-android" } }
		};

		// architecture paths to use for filtering include and lib paths
		static private Dictionary<string, string[]> AllFilterArchNames = new Dictionary<string, string[]> {
			{ "-armv7", new string[] { "armv7", "armeabi-v7a", "arm-android" } },
			{ "-arm64", new string[] { "arm64", "arm64-v8a", "arm64-android" } },
			{ "-x86",   new string[] { "x86", "x86-android" } },
			{ "-x64",   new string[] { "x64", "x86_64", "x64-android" } }
		};

		static private Dictionary<string, string[]> LibrariesToSkip = new Dictionary<string, string[]> {
			{ "-arm64", new string[] { "nvToolsExt", "nvToolsExtStub", "vorbisenc", } },
			{ "-x64",   new string[] { "nvToolsExt", "nvToolsExtStub", "oculus", "OVRPlugin", "vrapi", "ovrkernel", "systemutils", "openglloader", "ovrplatformloader", "gpg", "vorbisenc", } }
		};

		static private Dictionary<string, string[]> ModulesToSkip = new Dictionary<string, string[]> {
			{ "-arm64", new string[] {  } },
			{ "-x64",   new string[] { "OnlineSubsystemOculus", "OculusHMD", "OculusMR", "OnlineSubsystemGooglePlay" } }
		};

		static private Dictionary<string, string[]> GeneratedModulesToSkip = new Dictionary<string, string[]> {
			{ "-arm64", new string[] {  } },
			{ "-x64",   new string[] { "OculusEntitlementCallbackProxy", "OculusCreateSessionCallbackProxy", "OculusFindSessionsCallbackProxy", "OculusIdentityCallbackProxy", "OculusNetConnection", "OculusNetDriver", "OnlineSubsystemOculus_init" } }
		};

		public string? NDKToolchainVersion;
		public UInt64 NDKVersionInt;

		int ClangVersionMajor = -1;
		int ClangVersionMinor = -1;
		int ClangVersionPatch = -1;

		protected void SetClangVersion(int Major, int Minor, int Patch)
		{
			ClangVersionMajor = Major;
			ClangVersionMinor = Minor;
			ClangVersionPatch = Patch;
		}

		public string GetClangVersionString()
		{
			return string.Format("{0}.{1}.{2}", ClangVersionMajor, ClangVersionMinor, ClangVersionPatch);
		}

		public bool IsNewNDKModel()
		{
			// Google changed NDK structure in r22+
			return NDKVersionInt >= 220000;
		}

		[CommandLine("-Architectures=", ListSeparator = '+')]
		public List<string> ArchitectureArg = new List<string>();

		public AndroidToolChain(FileReference? InProjectFile, IReadOnlyList<string>? InAdditionalArches, IReadOnlyList<string>? InAdditionalGPUArches, ILogger InLogger)
			: this(InProjectFile, InAdditionalArches, InAdditionalGPUArches, false, ClangToolChainOptions.None, InLogger)
		{
		}

		public AndroidToolChain(FileReference? InProjectFile, IReadOnlyList<string>? InAdditionalArches, IReadOnlyList<string>? InAdditionalGPUArches, ClangToolChainOptions ToolchainOptions, ILogger InLogger)
			: this(InProjectFile, InAdditionalArches, InAdditionalGPUArches, false, ToolchainOptions, InLogger)
		{
		}

		protected AndroidToolChain(FileReference? InProjectFile, IReadOnlyList<string>? InAdditionalArches, IReadOnlyList<string>? InAdditionalGPUArches, bool bAllowMissingNDK, ClangToolChainOptions ToolchainOptions, ILogger InLogger)
			: base(ToolchainOptions, InLogger)
		{
			Options = ToolchainOptions;
			ProjectFile = InProjectFile;
			AdditionalArches = new List<string>();
			AdditionalGPUArches = new List<string>();

			if (InAdditionalArches != null)
			{
				AdditionalArches.AddRange(InAdditionalArches);
			}

			if (InAdditionalGPUArches != null)
			{
				AdditionalGPUArches.AddRange(InAdditionalGPUArches);
			}

			// by default tools chains don't parse arguments, but we want to be able to check the -architectures flag defined above. This is
			// only necessary when AndroidToolChain is used during UAT
			CommandLine.ParseArguments(Environment.GetCommandLineArgs(), this, Logger);

			if (AdditionalArches.Count == 0 && ArchitectureArg.Count > 0)
			{
				AdditionalArches.AddRange(ArchitectureArg);
			}

			string? NDKPath = Environment.GetEnvironmentVariable("NDKROOT");

			// don't register if we don't have an NDKROOT specified
			if (String.IsNullOrEmpty(NDKPath))
			{
				if (bAllowMissingNDK)
				{
					return;
				}
				throw new BuildException("NDKROOT is not specified; cannot use Android toolchain.");
			}

			NDKPath = NDKPath.Replace("\"", "");

			string ArchitecturePath;
			string ArchitecturePathWindows32 = @"prebuilt/windows";
			string ArchitecturePathWindows64 = @"prebuilt/windows-x86_64";
			string ArchitecturePathMac = @"prebuilt/darwin-x86_64";
			string ArchitecturePathLinux = @"prebuilt/linux-x86_64";
			string ExeExtension = ".exe";

			if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePathWindows64)))
			{
				Logger.LogDebug("        Found Windows 64 bit versions of toolchain");
				ArchitecturePath = ArchitecturePathWindows64;
			}
			else if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePathWindows32)))
			{
				Logger.LogDebug("        Found Windows 32 bit versions of toolchain");
				ArchitecturePath = ArchitecturePathWindows32;
			}
			else if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePathMac)))
			{
				Logger.LogDebug("        Found Mac versions of toolchain");
				ArchitecturePath = ArchitecturePathMac;
				ExeExtension = "";
			}
			else if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePathLinux)))
			{
				Logger.LogDebug("        Found Linux versions of toolchain");
				ArchitecturePath = ArchitecturePathLinux;
				ExeExtension = "";
			}
			else
			{
				throw new BuildException("Couldn't find 32-bit or 64-bit versions of the Android toolchain with NDKROOT: " + NDKPath);
			}

			// get the installed version (in the form r10e and 100500)
			UEBuildPlatformSDK SDK = UEBuildPlatform.GetSDK(UnrealTargetPlatform.Android)!;
			NDKToolchainVersion = SDK.GetInstalledVersion();
			SDK.TryConvertVersionToInt(NDKToolchainVersion, out NDKVersionInt);

			// figure out clang version (will live in toolchains/llvm from NDK 21 forward
			if (Directory.Exists(Path.Combine(NDKPath, @"toolchains/llvm")))
			{
				// look for version in AndroidVersion.txt (fail if not found)
				string VersionFilename = Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePath, "AndroidVersion.txt");
				if (!File.Exists(VersionFilename))
				{
					throw new BuildException("Cannot find supported Android toolchain");
				}
				string[] VersionFile = File.ReadAllLines(VersionFilename);
				string[] VersionParts = VersionFile[0].Split('.');
				SetClangVersion(int.Parse(VersionParts[0]), (VersionParts.Length > 1) ? int.Parse(VersionParts[1]) : 0, (VersionParts.Length > 2) ? int.Parse(VersionParts[2]) : 0);
			}
			else
			{
				throw new BuildException("Cannot find supported Android toolchain with NDKPath:" + NDKPath);
			}

			// set up the path to our toolchains
			ClangPath = Utils.CollapseRelativeDirectories(Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePath, @"bin/clang++" + ExeExtension));

			// Android (6317467 based on r365631c1) clang version 9.0.8 
			AndroidClangBuild = Utils.RunLocalProcessAndReturnStdOut(ClangPath, "--version", Logger);
			try
			{
				AndroidClangBuild = Regex.Match(AndroidClangBuild, @"(\w+) based on").Groups[1].ToString();
				if (String.IsNullOrEmpty(AndroidClangBuild))
				{
					AndroidClangBuild = Regex.Match(AndroidClangBuild, @"(\w+), based on").Groups[1].ToString();
				}
			}
			catch
			{
				Logger.LogWarning("Failed to retreive build version from {AndroidClangBuild}", AndroidClangBuild);
				AndroidClangBuild = "unknown";
			}

			// use lld for r21+
			ArPathArm64 = Utils.CollapseRelativeDirectories(Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePath, @"bin/llvm-ar" + ExeExtension));
			ArPathx64 = ArPathArm64;

			// NDK setup (enforce minimum API level)
			int NDKApiLevel64Int = GetNdkApiLevelInt(MinimumNDKAPILevel);

			string GCCToolchainPath = Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePath);
			string SysrootPath = Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePath, "sysroot");

			// toolchain params (note: use ANDROID=1 same as we define it)
			ToolchainLinkParamsArm64 = " --target=aarch64-none-linux-android" + NDKApiLevel64Int + " --gcc-toolchain=\"" + GCCToolchainPath + "\" --sysroot=\"" + SysrootPath + "\" -DANDROID=1";
			ToolchainLinkParamsx64 = " --target=x86_64-none-linux-android" + NDKApiLevel64Int + " --gcc-toolchain=\"" + GCCToolchainPath + "\" --sysroot=\"" + SysrootPath + "\" -DANDROID=1";

			ToolchainParamsArm64 = ToolchainLinkParamsArm64;
			ToolchainParamsx64 = ToolchainLinkParamsx64;

			if (!IsNewNDKModel())
			{
				// We need to manually provide -D__ANDROID_API__ for NDK versions prior to r22 only, for newer ones, --target=aarch64-none-linux-android + NDKApiLevel64Int does it for us
				ToolchainParamsArm64 += " -D__ANDROID_API__=" + NDKApiLevel64Int;
				ToolchainParamsx64 += " -D__ANDROID_API__=" + NDKApiLevel64Int;
			}

			ReadElfPath = Path.Combine(NDKPath, @"toolchains/llvm", ArchitecturePath, @"bin/llvm-readelf" + ExeExtension);
		}

		protected override ClangToolChainInfo GetToolChainInfo()
		{
			return new ClangToolChainInfo(FileReference.FromString(ClangPath)!, FileReference.FromString(ArPathArm64)!, Logger);
		}

		public virtual void ParseArchitectures()
		{
			// look in ini settings for what platforms to compile for
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
			Arches = new List<string>();
			bool bBuild = true;
			bool bUnsupportedBinaryBuildArch = false;

			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForArm64", out bBuild) && bBuild
				|| (AdditionalArches != null && (AdditionalArches.Contains("arm64", StringComparer.OrdinalIgnoreCase) || AdditionalArches.Contains("-arm64", StringComparer.OrdinalIgnoreCase))))
			{
				Arches.Add("-arm64");
			}
			if (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildForx8664", out bBuild) && bBuild
				|| (AdditionalArches != null && (AdditionalArches.Contains("x64", StringComparer.OrdinalIgnoreCase) || AdditionalArches.Contains("-x64", StringComparer.OrdinalIgnoreCase))))
			{
				if (File.Exists(Path.Combine(Unreal.EngineDirectory.FullName, "Build", "InstalledBuild.txt")))
				{
					bUnsupportedBinaryBuildArch = true;
					Log.TraceWarningOnce("Please install source to build for x86_64 (-x64); ignoring this architecture target.");
				}
				else
				{
					Arches.Add("-x64");
				}
			}

			// force armv7 if something went wrong
			if (Arches.Count == 0)
			{
				if (bUnsupportedBinaryBuildArch)
				{
					throw new BuildException("Only architectures unsupported by binary-only engine selected.");
				}
				else
				{
					Arches.Add("-arm64");
				}
			}
		}

		static public string GetGLESVersion(bool bBuildForES31)
		{
			string GLESversion = "0x00030000";

			if (bBuildForES31)
			{
				GLESversion = "0x00030002";
			}

			return GLESversion;
		}

		private bool BuildWithHiddenSymbolVisibility(CppCompileEnvironment CompileEnvironment)
		{
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
			bool bBuild = false;
			return CompileEnvironment.Configuration == CppConfiguration.Shipping && (Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bBuildWithHiddenSymbolVisibility", out bBuild) && bBuild);
		}

		private bool DisableFunctionDataSections()
		{
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
			bool bDisableFunctionDataSections = false;
			return Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bDisableFunctionDataSections", out bDisableFunctionDataSections) && bDisableFunctionDataSections;
		}

		private bool EnableAdvancedBinaryCompression()
		{
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
			bool bEnableAdvancedBinaryCompression = false;
			return Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bEnableAdvancedBinaryCompression", out bEnableAdvancedBinaryCompression) && bEnableAdvancedBinaryCompression;
		}

		private bool DisableStackProtector()
		{
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
			bool bDisableStackProtector = false;
			return Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bDisableStackProtector", out bDisableStackProtector) && bDisableStackProtector;
		}

		private bool DisableLibCppSharedDependencyValidation()
		{
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
			bool bDisableLibCppSharedDependencyValidation = false;
			return Ini.GetBool("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "bDisableLibCppSharedDependencyValidation", out bDisableLibCppSharedDependencyValidation) && bDisableLibCppSharedDependencyValidation;
		}

		private string GetVersionScriptFilename(LinkEnvironment LinkEnvironment)
		{
			return Path.Combine(LinkEnvironment.IntermediateDirectory!.FullName, "ExportSymbols.ldscript");
		}

		public override void SetUpGlobalEnvironment(ReadOnlyTargetRules Target)
		{
			base.SetUpGlobalEnvironment(Target);

			ParseArchitectures();
		}

		public List<string> GetAllArchitectures()
		{
			if (Arches == null)
			{
				ParseArchitectures();
			}

			return Arches!;
		}

		public int GetNdkApiLevelInt(int MinNdk = MinimumNDKAPILevel)
		{
			string NDKVersion = GetNdkApiLevel();
			int NDKVersionInt = MinNdk;
			if (NDKVersion.Contains("-"))
			{
				int Version;
				if (int.TryParse(NDKVersion.Substring(NDKVersion.LastIndexOf('-') + 1), out Version))
				{
					if (Version > NDKVersionInt)
						NDKVersionInt = Version;
				}
			}
			return NDKVersionInt;
		}

		public UInt64 GetNdkVersionInt()
		{
			return NDKVersionInt;
		}

		static string CachedPlatformsFilename = "";
		static bool CachedPlatformsValid = false;
		static int CachedMinPlatform = -1;
		static int CachedMaxPlatform = -1;

		private bool ReadMinMaxPlatforms(string PlatformsFilename, out int MinPlatform, out int MaxPlatform)
		{
			if (!CachedPlatformsFilename.Equals(PlatformsFilename))
			{
				// reset cache to defaults
				CachedPlatformsFilename = PlatformsFilename;
				CachedPlatformsValid = false;
				CachedMinPlatform = -1;
				CachedMaxPlatform = -1;

				// try to read it
				try
				{
					JsonObject? PlatformsObj = null;
					if (JsonObject.TryRead(new FileReference(PlatformsFilename), out PlatformsObj))
					{
						CachedPlatformsValid = PlatformsObj.TryGetIntegerField("min", out CachedMinPlatform) && PlatformsObj.TryGetIntegerField("max", out CachedMaxPlatform);
					}
				}
				catch (Exception)
				{
				}
			}

			MinPlatform = CachedMinPlatform;
			MaxPlatform = CachedMaxPlatform;
			return CachedPlatformsValid;
		}
		
		//This doesn't take into account SDK version overrides in packaging
		public int GetMinSdkVersion(int MinSdk = MinimumNDKAPILevel)
		{
			int MinSDKVersion = MinSdk;
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
			Ini.GetInt32("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "MinSDKVersion", out MinSDKVersion);
			return MinSDKVersion;
		}

		protected virtual bool ValidateNDK(string PlatformsFilename, string ApiString)
		{
			int MinPlatform, MaxPlatform;
			if (!ReadMinMaxPlatforms(PlatformsFilename, out MinPlatform, out MaxPlatform))
			{
				return false;
			}

			if (ApiString.Contains("-"))
			{
				int Version;
				if (int.TryParse(ApiString.Substring(ApiString.LastIndexOf('-') + 1), out Version))
				{
					return (Version >= MinPlatform && Version <= MaxPlatform);
				}
			}
			return false;
		}

		public virtual string GetNdkApiLevel()
		{
			// ask the .ini system for what version to use
			ConfigHierarchy Ini = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, DirectoryReference.FromFile(ProjectFile), UnrealTargetPlatform.Android);
			string NDKLevel;
			Ini.GetString("/Script/AndroidPlatformEditor.AndroidSDKSettings", "NDKAPILevel", out NDKLevel!);

			// check for project override of NDK API level
			string ProjectNDKLevel;
			Ini.GetString("/Script/AndroidRuntimeSettings.AndroidRuntimeSettings", "NDKAPILevelOverride", out ProjectNDKLevel!);
			ProjectNDKLevel = ProjectNDKLevel.Trim();
			if (ProjectNDKLevel != "")
			{
				NDKLevel = ProjectNDKLevel;
			}

			string PlatformsFilename = Environment.ExpandEnvironmentVariables("%NDKROOT%/meta/platforms.json");
			if (!File.Exists(PlatformsFilename))
			{
				throw new BuildException("No NDK platforms found in {0}", PlatformsFilename);
			}

			if (NDKLevel == "latest")
			{
				int MinPlatform, MaxPlatform;
				if (!ReadMinMaxPlatforms(PlatformsFilename, out MinPlatform, out MaxPlatform))
				{
					throw new BuildException("No NDK platforms found in {0}", PlatformsFilename);
				}

				NDKLevel = "android-" + MaxPlatform.ToString();
			}

			// validate the platform NDK is installed
			if (!ValidateNDK(PlatformsFilename, NDKLevel))
			{
				throw new BuildException("The NDK API requested '{0}' not installed in {1}", NDKLevel, PlatformsFilename);
			}

			return NDKLevel;
		}

		public string GetLargestApiLevel()
		{
			string PlatformsFilename = Environment.ExpandEnvironmentVariables("%NDKROOT%/meta/platforms.json");
			if (!File.Exists(PlatformsFilename))
			{
				throw new BuildException("No NDK platforms found in {0}", PlatformsFilename);
			}

			int MinPlatform, MaxPlatform;
			if (!ReadMinMaxPlatforms(PlatformsFilename, out MinPlatform, out MaxPlatform))
			{
				throw new BuildException("No NDK platforms found in {0}", PlatformsFilename);
			}

			MaxPlatform = Math.Clamp(MaxPlatform, 1, 32);	//Current NDK 25beta4 breaks existing UE NDK to SDK API mapping, so clamp it at 32

			return "android-" + MaxPlatform.ToString();
		}

		protected virtual string GetCLArguments_Global(CppCompileEnvironment CompileEnvironment, string Architecture)
		{
			string Result = "";

			switch (Architecture)
			{
				case "-arm64": Result += ToolchainParamsArm64; break;
				case "-x64": Result += ToolchainParamsx64; break;
				default: Result += ToolchainParamsArm64; break;
			}

			// build up the commandline common to C and C++
			Result += " -c";
			Result += " -no-canonical-prefixes";
			Result += " -fdiagnostics-format=msvc";
			Result += " -Wall";
			Result += " -Wdelete-non-virtual-dtor";
			Result += " -fno-PIE";

			Result += " -Wno-unused-variable";
            if (CompilerVersionGreaterOrEqual(13, 0, 0))
            {
                Result += " -Wno-unused-but-set-variable";
                Result += " -Wno-unused-but-set-parameter";
                Result += " -Wno-ordered-compare-function-pointers";
            }
			
			if (CompilerVersionGreaterOrEqual(14, 0, 2))
			{
				Result += " -Wno-bitwise-instead-of-logical";
			}

			// this will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Result += " -Wno-unused-function";
			// this hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Result += " -Wno-switch";
			// this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
			Result += " -Wno-tautological-compare";
			//This will prevent the issue of warnings for unused private variables.
			Result += " -Wno-unused-private-field";
			Result += " -Wno-local-type-template-args"; // engine triggers this
			Result += " -Wno-return-type-c-linkage";    // needed for PhysX
			Result += " -Wno-reorder";                  // member initialization order
			Result += " -Wno-unknown-pragmas";          // probably should kill this one, sign of another issue in PhysX?
			Result += " -Wno-invalid-offsetof";         // needed to suppress warnings about using offsetof on non-POD types.
			Result += " -Wno-logical-op-parentheses";   // needed for external headers we can't change

			if (BuildWithHiddenSymbolVisibility(CompileEnvironment))
			{
				// Result += " -fvisibility-ms-compat -fvisibility-inlines-hidden"; // This hides all symbols by default but exports all type info (vtable/rtti) for a non-monolithic setup
				Result += " -fvisibility=hidden -fvisibility-inlines-hidden"; // Symbols default to hidden.
				//TODO: add when Android's clang will support this
				//Resul += " -fvisibility-inlines-hidden-static-local-var";
			}

			if (CompilerVersionGreaterOrEqual(12, 0, 0))
			{
				// The 'this' vs nullptr comparisons get optimized away for newer versions of Clang, which is undesirable until we refactor these checks.
				Result += " -fno-delete-null-pointer-checks";
			}

			if (CompileEnvironment.DeprecationWarningLevel == WarningLevel.Error)
			{
				Result += " -Werror=deprecated-declarations";
			}

			if (CompileEnvironment.ShadowVariableWarningLevel != WarningLevel.Off)
			{
				Result += " -Wshadow" + ((CompileEnvironment.ShadowVariableWarningLevel == WarningLevel.Error) ? "" : " -Wno-error=shadow");
			}

			if (CompileEnvironment.bEnableUndefinedIdentifierWarnings)
			{
				Result += " -Wundef" + (CompileEnvironment.bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef");
			}

			// new for clang4.5 warnings:
			if (CompilerVersionGreaterOrEqual(3, 5, 0))
			{
				Result += " -Wno-undefined-bool-conversion"; // 'this' pointer cannot be null in well-defined C++ code; pointer may be assumed to always convert to true (if (this))

				// we use this feature to allow static FNames.
				Result += " -Wno-gnu-string-literal-operator-template";
			}

			if (CompilerVersionGreaterOrEqual(3, 6, 0))
			{
				Result += " -Wno-unused-local-typedef";             // clang is being overly strict here? PhysX headers trigger this.
				Result += " -Wno-inconsistent-missing-override";    // these have to be suppressed for UE 4.8, should be fixed later.
			}

			if (CompilerVersionGreaterOrEqual(3, 8, 275480))
			{
				Result += " -Wno-undefined-var-template";           // not really a good warning to disable
				Result += " -Wno-nonportable-include-path";         // not all of these are real
			}

            if (CompilerVersionGreaterOrEqual(4, 0, 0))
			{
				Result += " -Wno-unused-lambda-capture";            // probably should fix the code
																	//				Result += " -Wno-nonportable-include-path";         // not all of these are real
			}

			// shipping builds will cause this warning with "ensure", so disable only in those case
			if (CompileEnvironment.Configuration == CppConfiguration.Shipping)
			{
				Result += " -Wno-unused-value";
			}

			// debug info
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Result += " -g2 -gdwarf-4";
			}

			// optimization level
			if (!CompileEnvironment.bOptimizeCode)
			{
				Result += " -O0";
			}
			else
			{
				if (CompileEnvironment.OptimizationLevel == OptimizationMode.Size)
				{
					Result += " -Oz";
				}
				else if (CompileEnvironment.OptimizationLevel == OptimizationMode.SizeAndSpeed)
				{
					Result += " -Os";
					if (Architecture == "-arm64")
					{
						Result += " -moutline";
					}
				}
				else
				{
					Result += " -O3";
				}
			}

			// FORTIFY default
			Result += " -D_FORTIFY_SOURCE=2";

			// Optionally enable exception handling (off by default since it generates extra code needed to propagate exceptions)
			if (CompileEnvironment.bEnableExceptions)
			{
				Result += " -fexceptions";
				Result += " -DPLATFORM_EXCEPTIONS_DISABLED=0";
			}
			else
			{
				Result += " -fno-exceptions";
				Result += " -DPLATFORM_EXCEPTIONS_DISABLED=1";
			}

			Result += $" {GetRTTIFlag(CompileEnvironment)}";

			// Profile Guided Optimization (PGO) and Link Time Optimization (LTO)
			if (CompileEnvironment.bPGOOptimize)
			{
				//
				// Clang emits warnings for each compiled object file that doesn't have a matching entry in the profile data.
				// This can happen when the profile data is older than the binaries we're compiling.
				//
				// Disable these warnings. They are far too verbose.
				//
				Result += " -Wno-profile-instr-out-of-date";
				Result += " -Wno-profile-instr-unprofiled";

				// apparently there can be hashing conflicts with PGO which can result in:
				// 'Function control flow change detected (hash mismatch)' warnings. 
				Result += " -Wno-backend-plugin";

				Result += string.Format(" -fprofile-use=\"{0}.profdata\"", Path.Combine(CompileEnvironment.PGODirectory!, CompileEnvironment.PGOFilenamePrefix!));
			}
			else if (CompileEnvironment.bPGOProfile)
			{
				// Always enable LTO when generating PGO profile data.
				// Android supports only LLVM IR-based for instrumentation-based PGO.
				Result += " -fprofile-generate";
				//  for sampling-based profile collection to generate minimal debug information:
				Result += " -gline-tables-only";
			}

			if (CompileEnvironment.bAllowLTCG)
			{
				if ((Options & ClangToolChainOptions.EnableThinLTO) != 0)
				{
					Result += " -flto=thin";
				}
				else
				{
					Result += " -flto";
				}
			}

			if (Architecture == "-arm64")
			{
				Result += " -funwind-tables";           // Just generates any needed static data, affects no code
				Result += " -fno-strict-aliasing";      // Prevents unwanted or invalid optimizations that could produce incorrect code
				Result += " -fPIC";                     // Generates position-independent code (PIC) suitable for use in a shared library
				Result += " -fno-short-enums";          // Do not allocate to an enum type only as many bytes as it needs for the declared range of possible values
				Result += " -D__arm64__";               // for some reason this isn't defined and needed for PhysX

				Result += " -march=armv8-a";
				//Result += " -mfloat-abi=softfp";
				//Result += " -mfpu=vfpv3-d16";			//@todo android: UE3 was just vfp. arm7a should all support v3 with 16 registers

				// Add flags for on-device debugging
				if (CompileEnvironment.Configuration == CppConfiguration.Debug)
				{
					Result += " -fno-omit-frame-pointer";   // Disable removing the save/restore frame pointer for better debugging
					if (CompilerVersionGreaterOrEqual(3, 6, 0))
					{
						Result += " -fno-function-sections";    // Improve breakpoint location
					}
				}

				// Some switches interfere with on-device debugging
				if (CompileEnvironment.Configuration != CppConfiguration.Debug && !DisableFunctionDataSections())
				{
					Result += " -ffunction-sections";   // Places each function in its own section of the output file, linker may be able to perform opts to improve locality of reference
					Result += " -fdata-sections";		// Places each data item in its own section of the output file, linker may be able to perform opts to improve locality of reference
				}

				Result += " -fsigned-char";             // Treat chars as signed //@todo android: any concerns about ABI compatibility with libs here?
			}
			else if (Architecture == "-x64")
			{
				Result += " -funwind-tables";           // Just generates any needed static data, affects no code
				Result += " -fPIC";                     // Generates position-independent code (PIC) suitable for use in a shared library
				Result += " -fno-omit-frame-pointer";
				Result += " -fno-strict-aliasing";
				Result += " -fno-short-enums";
				Result += " -march=atom";
			}

			ClangSanitizer Sanitizer = BuildWithSanitizer();
			if (Sanitizer != ClangSanitizer.None)
			{
				Result += " -fsanitize=" + GetCompilerOption(Sanitizer);

				if (Sanitizer == ClangSanitizer.Address || Sanitizer == ClangSanitizer.HwAddress)
				{
					Result += " -fno-omit-frame-pointer -DRUNNING_WITH_ASAN=1";
				}
			}

			if (!DisableStackProtector())
			{
				Result += " -fstack-protector-strong";  // Emits extra code to check for buffer overflows
			}

			// Add analysis flags to the argument list.
			if (IsAnalyzing(CompileEnvironment))
			{
				List<string> Arguments = new List<string>();
				GetCompileArguments_Analyze(CompileEnvironment, Arguments);
				for (int i = 0; i < Arguments.Count; i++)
				{
					Result += " " + Arguments[i];
				}
			}

			Result += " -fforce-emit-vtables";      // Helps with devirtualization
			//Result += " -fstrict-vtable-pointers";	// Assumes that vtable pointer will never change, i.e. object of a different type would not be constracted in place of another object. Helps with devirtualization

			return Result;
		}

		protected virtual string GetLinkArguments(LinkEnvironment LinkEnvironment, string Architecture)
		{
			string Result = "";

			//Result += " -nostdlib";
			Result += " -static-libstdc++";
			Result += " -no-canonical-prefixes";
			Result += " -shared";
			Result += " -Wl,-Bsymbolic";
			Result += " -Wl,--no-undefined";
			if (!DisableFunctionDataSections())
			{
				Result += " -Wl,--gc-sections"; // Enable garbage collection of unused input sections. works best with -ffunction-sections, -fdata-sections
			}

			if (!LinkEnvironment.bCreateDebugInfo)
			{
				Result += " -Wl,--strip-debug";
			}

			if (Architecture == "-x64")
			{
				Result += ToolchainLinkParamsx64;
				Result += " -march=atom";
			}
			else // if (Architecture == "-arm64")
			{
				Result += ToolchainLinkParamsArm64;
				Result += " -march=armv8-a";
			}

			if (LinkEnvironment.Configuration == CppConfiguration.Shipping)
			{
				Result += " -Wl,--icf=all"; // Enables ICF (Identical Code Folding). [all, safe] safe == fold functions that can be proven not to have their address taken.
				Result += " -Wl,-O3";
			}

			Result += " -Wl,-no-pie";

			// use lld as linker (requires llvm-strip)
			Result += " -fuse-ld=lld";


			// make sure the DT_SONAME field is set properly (or we can a warning toast at startup on new Android)
			Result += " -Wl,-soname,libUnreal.so";

			if (LinkEnvironment.Configuration == CppConfiguration.Shipping)
			{
				string VersionScriptFile = GetVersionScriptFilename(LinkEnvironment);
				using (StreamWriter Writer = File.CreateText(VersionScriptFile))
				{
					// Make all symbols (except ones called from Java) hidden
					Writer.WriteLine("{ global: Java_*; ANativeActivity_onCreate; JNI_OnLoad; local: *; };");
				}
				Result += " -Wl,--version-script=\"" + VersionScriptFile + "\"";
			}

			Result += " -Wl,--build-id=sha1";               // add build-id to make debugging easier

			if (LinkEnvironment.bPGOOptimize)
			{
				//
				// Clang emits warnings for each compiled object file that doesn't have a matching entry in the profile data.
				// This can happen when the profile data is older than the binaries we're compiling.
				//
				// Disable these warnings. They are far too verbose.
				//
				Result += " -Wno-profile-instr-out-of-date";
				Result += " -Wno-profile-instr-unprofiled";

				Result += string.Format(" -fprofile-use=\"{0}.profdata\"", Path.Combine(LinkEnvironment.PGODirectory!, LinkEnvironment.PGOFilenamePrefix!));
			}
			else if (LinkEnvironment.bPGOProfile)
			{
				// Android supports only LLVM IR-based for instrumentation-based PGO.
				Result += " -fprofile-generate";
				//  for sampling-based profile collection to generate minimal debug information:
				Result += " -gline-tables-only";
			}

			if (LinkEnvironment.bAllowLTCG)
			{
				if ((Options & ClangToolChainOptions.EnableThinLTO) != 0)
				{
					Result += " -flto=thin";
				}
				else
				{
					Result += " -flto";
				}
			}

			// verbose output from the linker
			// Result += " -v";

			ClangSanitizer Sanitizer = BuildWithSanitizer();
			if (Sanitizer != ClangSanitizer.None)
			{
				Result += " -fsanitize=" + GetCompilerOption(Sanitizer);
			}

			if (EnableAdvancedBinaryCompression())
			{
				int MinSDKVersion = GetMinSdkVersion();
				if (MinSDKVersion >= 28)
				{
					//Pack relocations in RELR format and Android APS2 packed format for RELA relocations if they can't be expressed in RELR
					Result += " -Wl,--pack-dyn-relocs=android+relr,--use-android-relr-tags";
				}
				else if (MinSDKVersion >= 23)
				{
					Result += " -Wl,--pack-dyn-relocs=android";
				}

				if (MinSDKVersion >= 23)
				{
					Result += " -Wl,--hash-style=gnu";  // generate GNU style hashes, faster lookup and faster startup. Avoids generating old .hash section. Supported on >= Android M
				}
			}

			return Result;
		}


		protected virtual void ModifyLibraries(LinkEnvironment LinkEnvironment)
		{
			// @todo Lumin: verify this works with base android
			if (GetNdkApiLevelInt() >= 21)
			{
				// this file was added in NDK11 so use existence to detect (RELEASE.TXT no longer present)
				//string NDKRoot = Environment.GetEnvironmentVariable("NDKROOT").Replace("\\", "/");
			}
		}

		static string GetArArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			Result += " -r";

			return Result;
		}

		static bool IsDirectoryForArch(string Dir, string Arch)
		{
			// make sure paths use one particular slash
			Dir = Dir.Replace("\\", "/").ToLowerInvariant();

			// look for other architectures in the Dir path, and fail if it finds it
			foreach (KeyValuePair<string, string[]> Pair in AllFilterArchNames)
			{
				if (Pair.Key != Arch)
				{
					foreach (string ArchName in Pair.Value)
					{
						// if there's a directory in the path with a bad architecture name, reject it
						if (Regex.IsMatch(Dir, "/" + ArchName + "$") || Regex.IsMatch(Dir, "/" + ArchName + "/") || Regex.IsMatch(Dir, "/" + ArchName + "_API[0-9]+_NDK[0-9]+", RegexOptions.IgnoreCase))
						{
							return false;
						}
					}
				}
			}

			// if nothing was found, we are okay
			return true;
		}

		static bool ShouldSkipModule(string ModuleName, string Arch)
		{
			foreach (string ModName in ModulesToSkip[Arch])
			{
				if (ModName == ModuleName)
				{
					return true;
				}
			}

			// if nothing was found, we are okay
			return false;
		}

		bool ShouldSkipLib(string FullLib, string Arch)
		{
			// strip any absolute path
			string Lib = Path.GetFileNameWithoutExtension(FullLib);
			if (Lib.StartsWith("lib"))
			{
				Lib = Lib.Substring(3);
			}

			// reject any libs we outright don't want to link with
			foreach (string LibName in LibrariesToSkip[Arch])
			{
				if (LibName == Lib)
				{
					return true;
				}
			}

			// deal with .so files with wrong architecture
			if (Path.GetExtension(FullLib) == ".so")
			{
				string ParentDirectory = Path.GetDirectoryName(FullLib)!;
				if (!IsDirectoryForArch(ParentDirectory, Arch))
				{
					return true;
				}
			}

			// apply the same directory filtering to libraries as we do to additional library paths
			if (!IsDirectoryForArch(Path.GetDirectoryName(FullLib)!, Arch))
			{
				return true;
			}

			// if another architecture is in the filename, reject it
			foreach (string ComboName in Arches!)
			{
				if (ComboName != Arch)
				{
					if (Lib.EndsWith(ComboName))
					{
						return true;
					}
				}
			}

			// if nothing was found, we are okay
			return false;
		}

		static private string GetNativeGluePath()
		{
			return Environment.GetEnvironmentVariable("NDKROOT") + "/sources/android/native_app_glue/android_native_app_glue.c";
		}

		static private string GetCpuFeaturesPath()
		{
			return Environment.GetEnvironmentVariable("NDKROOT") + "/sources/android/cpufeatures/cpu-features.c";
		}

		protected virtual void ModifySourceFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> SourceFiles, string ModuleName)
		{
			// We need to add the extra glue and cpu code only to Launch module.
			if (ModuleName.Equals("Launch") || ModuleName.Equals("AndroidLauncher"))
			{
				SourceFiles.Add(FileItem.GetItemByPath(GetNativeGluePath()));
				SourceFiles.Add(FileItem.GetItemByPath(GetCpuFeaturesPath()));
			}
		}

		void GenerateEmptyLinkFunctionsForRemovedModules(List<FileItem> SourceFiles, string ModuleName, DirectoryReference OutputDirectory, IActionGraphBuilder Graph, ILogger Logger)
		{
			// Only add to Launch module
			if (!ModuleName.Equals("Launch"))
			{
				return;
			}

			string LinkerExceptionsName = "../UELinkerExceptions";
			FileReference LinkerExceptionsCPPFilename = FileReference.Combine(OutputDirectory, LinkerExceptionsName + ".cpp");

			List<string> Result = new List<string>();
			Result.Add("#include \"CoreTypes.h\"");
			Result.Add("");
			foreach (string Arch in Arches!)
			{
				switch (Arch)
				{
					case "-arm64": Result.Add("#if PLATFORM_ANDROID_ARM64"); break;
					case "-x64": Result.Add("#if PLATFORM_ANDROID_X64"); break;
					default: Result.Add("#if PLATFORM_ANDROID_ARM"); break;
				}

				foreach (string ModName in ModulesToSkip[Arch])
				{
					Result.Add("  void EmptyLinkFunctionForStaticInitialization" + ModName + "(){}");
				}
				foreach (string ModName in GeneratedModulesToSkip[Arch])
				{
					Result.Add("  void EmptyLinkFunctionForGeneratedCode" + ModName + "(){}");
				}
				Result.Add("#endif");
			}

			Graph.CreateIntermediateTextFile(LinkerExceptionsCPPFilename, Result);

			SourceFiles.Add(FileItem.GetItemByFileReference(LinkerExceptionsCPPFilename));
		}

		// cache the location of NDK tools
		protected static string? ClangPath;
		protected static string? ToolchainParamsArm64;
		protected static string? ToolchainParamsx64;
		protected static string? ToolchainLinkParamsArm64;
		protected static string? ToolchainLinkParamsx64;
		protected static string? ArPathArm64;
		protected static string? ArPathx64;
		protected static string? ReadElfPath;

		static public string GetStripExecutablePath(string UnrealArch)
		{
			string StripPath;

			switch (UnrealArch)
			{
				case "-arm64": StripPath = ArPathArm64!; break;
				case "-x64": StripPath = ArPathx64!; break;
				default: StripPath = ArPathArm64!; break;
			}
			return StripPath.Replace("-ar", "-strip");
		}

		static private bool bHasHandledLaunchModule = false;
		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph)
		{
			if (Arches!.Count == 0)
			{
				throw new BuildException("At least one architecture (arm64, x64, etc) needs to be selected in the project settings to build");
			}

			CPPOutput Result = new CPPOutput();

			// Skip if nothing to do
			if (InputFiles.Count == 0)
			{
				return Result;
			}

			/*
			Trace.TraceInformation("CompileCPPFiles: Module={0}, SourceFiles={1}", ModuleName, SourceFiles.Count);
			foreach (string Arch in Arches)
			{
				Trace.TraceInformation("  Arch: {0}", Arch);
			}
			foreach (FileItem SourceFile in SourceFiles)
			{
				Trace.TraceInformation("  {0}", SourceFile.AbsolutePath);
			}
			*/

			// NDK setup (enforce minimum API level)
			int NDKApiLevel64Int = GetNdkApiLevelInt(21);		// deliberately use 21 minimum to force NDKApiLevel64Bit to update if below minimum
			string NDKApiLevel64Bit = GetNdkApiLevel();
			if (NDKApiLevel64Int < MinimumNDKAPILevel)
			{
				NDKApiLevel64Int = MinimumNDKAPILevel;
				NDKApiLevel64Bit = "android-" + MinimumNDKAPILevel;
			}

			Log.TraceInformationOnce("Compiling Native 64-bit code with NDK API '{0}'", NDKApiLevel64Bit);

			string BaseArguments = "";

			if (CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create)
			{
				BaseArguments += " -Werror";
			}

			string NativeGluePath = Path.GetFullPath(GetNativeGluePath());

			// Deal with Launch module special if first time seen
			if (!bHasHandledLaunchModule && (ModuleName.Equals("Launch") || ModuleName.Equals("AndroidLauncher")))
			{
				// Directly added NDK files for NDK extensions
				ModifySourceFiles(CompileEnvironment, InputFiles, ModuleName);
				// Deal with dynamic modules removed by architecture
				GenerateEmptyLinkFunctionsForRemovedModules(InputFiles, ModuleName, OutputDir, Graph, Logger);

				bHasHandledLaunchModule = true;
			}

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				// We must change the \" to escaped double quotes to save it properly for clang .rsp
				BaseArguments += string.Format(" -D \"{0}\"", Definition.Replace("\"", RuntimePlatform.IsWindows ? "\"\"" : "\\\""));
			}

			//LUMIN_MERGE
			//string NDKRoot = Environment.GetEnvironmentVariable("NDKROOT").Replace("\\", "/");

			string BasePCHName = "";
			//UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(CompileEnvironment.Platform);
			string PCHExtension = ".gch";
			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				BasePCHName = RemoveArchName(CompileEnvironment.PrecompiledHeaderFile!.AbsolutePath).Replace(PCHExtension, "");
			}

			// Create a compile action for each source file.
			foreach (string Arch in Arches)
			{
				if (ShouldSkipModule(ModuleName, Arch))
				{
					continue;
				}

				// which toolchain to use
				string Arguments = GetCLArguments_Global(CompileEnvironment, Arch) + BaseArguments;

				switch (Arch)
				{
					case "-x64": Arguments += " -DPLATFORM_64BITS=1 -DPLATFORM_ANDROID_X64=1 -DPLATFORM_USED_NDK_VERSION_INTEGER=" + NDKApiLevel64Int.ToString(); break;
					case "-arm64": Arguments += " -DPLATFORM_64BITS=1 -DPLATFORM_ANDROID_ARM64=1 -DPLATFORM_USED_NDK_VERSION_INTEGER=" + NDKApiLevel64Int.ToString(); break;
					default: throw new BuildException($"Unknown arch {Arch}");
				}

				if(CompileEnvironment.bCompileISPC)
				{
					Arguments += " -DINTEL_ISPC=1";
				}

				// which PCH file to include
				string PCHArguments = "";
				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
				{
					// add the platform-specific PCH reference
					PCHArguments += string.Format(" -include-pch \"{0}\"", InlineArchName(BasePCHName, Arch) + PCHExtension);

					// Add the precompiled header file's path to the include path so Clang can find it.
					// This needs to be before the other include paths to ensure Clang uses it instead of the source header file.
					PCHArguments += string.Format(" -include \"{0}\"", BasePCHName);

					// Validate PCH inputs by content if mtime check fails
					if (CompilerVersionGreaterOrEqual(11, 0, 0))
					{
						PCHArguments += " -fpch-validate-input-files-content";
					}
				}

				// Add include paths to the argument list (filtered by architecture)
				foreach (DirectoryReference IncludePath in CompileEnvironment.SystemIncludePaths)
				{
					if (IsDirectoryForArch(IncludePath.FullName, Arch))
					{
						Arguments += string.Format(" -I\"{0}\"", IncludePath);
					}
				}
				foreach (DirectoryReference IncludePath in CompileEnvironment.UserIncludePaths)
				{
					if (IsDirectoryForArch(IncludePath.FullName, Arch))
					{
						Arguments += string.Format(" -I\"{0}\"", IncludePath);
					}
				}

				foreach (FileItem SourceFile in InputFiles)
				{
					Action CompileAction = Graph.CreateAction(ActionType.Compile);
					CompileAction.PrerequisiteItems.AddRange(CompileEnvironment.ForceIncludeFiles);
					CompileAction.PrerequisiteItems.AddRange(CompileEnvironment.AdditionalPrerequisites);

					string FileArguments = "";
					bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";
					bool bDisableShadowWarning = false;

					// Add C or C++ specific compiler arguments.
					if (bIsPlainCFile)
					{
						List<string> CArguments = new();
						GetCompileArguments_C(CompileEnvironment, CArguments);
						FileArguments += $" {string.Join(' ', CArguments)}";

						// remove shadow variable warnings for externally included files
						if (!SourceFile.Location.IsUnderDirectory(Unreal.RootDirectory))
						{
							bDisableShadowWarning = true;
						}
					}
					else if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
					{
						List<string> CreatePCHArguments = new();
						GetCompileArguments_PCH(CompileEnvironment, CreatePCHArguments);
						FileArguments += $" {string.Join(' ', CreatePCHArguments)}";
					}
					else
					{
						List<string> CPPArguments = new();
						GetCompileArguments_CPP(CompileEnvironment, CPPArguments);
						FileArguments += $" {string.Join(' ', CPPArguments)}";

						// only use PCH for .cpp files
						FileArguments += PCHArguments;
					}

					foreach (FileItem ForceIncludeFile in CompileEnvironment.ForceIncludeFiles)
					{
						FileArguments += string.Format(" -include \"{0}\"", ForceIncludeFile.Location);
					}

					// Add the C++ source file and its included files to the prerequisite item list.
					CompileAction.PrerequisiteItems.Add(SourceFile);

					List<FileItem>? InlinedFiles;
					if (CompileEnvironment.FileInlineGenCPPMap.TryGetValue(SourceFile, out InlinedFiles))
					{
						CompileAction.PrerequisiteItems.AddRange(InlinedFiles);
					}

					if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create && !bIsPlainCFile)
					{
						// Add the precompiled header file to the produced item list.
						FileItem PrecompiledHeaderFile = FileItem.GetItemByFileReference(
							FileReference.Combine(
								OutputDir,
								Path.GetFileName(InlineArchName(SourceFile.AbsolutePath, Arch) + PCHExtension)
								)
							);

						CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
						Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

						// Add the parameters needed to compile the precompiled header file to the command-line.
						FileArguments += string.Format(" -o \"{0}\"", PrecompiledHeaderFile.AbsolutePath);
					}
					else
					{
						if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
						{
							FileItem ArchPrecompiledHeaderFile = FileItem.GetItemByPath(InlineArchName(BasePCHName, Arch) + PCHExtension);
							CompileAction.PrerequisiteItems.Add(ArchPrecompiledHeaderFile);
						}

						string ObjectFileExtension;
						if(CompileEnvironment.AdditionalArguments != null && CompileEnvironment.AdditionalArguments.Contains("-emit-llvm"))
						{
							ObjectFileExtension = ".bc";
						}
						else
						{
							ObjectFileExtension = ".o";
						}

						// Add the object file to the produced item list.
						FileItem ObjectFile = FileItem.GetItemByFileReference(
							FileReference.Combine(
								OutputDir,
								InlineArchName(Path.GetFileName(SourceFile.AbsolutePath) + ObjectFileExtension, Arch, true)
								)
							);
						CompileAction.ProducedItems.Add(ObjectFile);
						Result.ObjectFiles.Add(ObjectFile);

						FileArguments += string.Format(" -o \"{0}\"", ObjectFile.AbsolutePath);
					}

					// Add the source file path to the command-line.
					FileArguments += string.Format(" \"{0}\"", SourceFile.AbsolutePath);

					// Generate the timing info
					if (CompileEnvironment.bPrintTimingInfo)
					{
						bool ShortArchName = !(CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create && !bIsPlainCFile);
						FileItem TraceFile;
						if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create && !bIsPlainCFile)
						{
							TraceFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, InlineArchName(Path.GetFileName(SourceFile.AbsolutePath), Arch) + ".json"));
						}
						else
						{
							TraceFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, InlineArchName(Path.GetFileName(SourceFile.AbsolutePath) + ".json", Arch, true)));
						}
						FileArguments += " -ftime-trace";
						CompileAction.ProducedItems.Add(TraceFile);
					}

					// Generate the included header dependency list
					if (CompileEnvironment.bGenerateDependenciesFile)
					{
						FileItem DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, InlineArchName(Path.GetFileName(SourceFile.AbsolutePath) + ".d", Arch, true)));
						FileArguments += string.Format(" -MD -MF\"{0}\"", DependencyListFile.AbsolutePath.Replace('\\', '/'));
						CompileAction.DependencyListFile = DependencyListFile;
						CompileAction.ProducedItems.Add(DependencyListFile);
					}

					// Build a full argument list
					string AllArguments = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;

					if (SourceFile.AbsolutePath.Equals(NativeGluePath))
					{
						// Remove visibility settings for android native glue. Since it doesn't decorate with visibility attributes.
						//AllArguments = AllArguments.Replace("-fvisibility-ms-compat -fvisibility-inlines-hidden", "");
						AllArguments = AllArguments.Replace("-fvisibility=hidden -fvisibility-inlines-hidden", "");
					}

					AllArguments = Utils.ExpandVariables(AllArguments);
					AllArguments = AllArguments.Replace("\\", "/");

					// Remove shadow warning for this file if requested
					if (bDisableShadowWarning)
					{
						int WarningIndex = AllArguments.IndexOf(" -Wshadow");
						if (WarningIndex > 0)
						{
							AllArguments = AllArguments.Remove(WarningIndex, 9);
						}
					}

					// Create the response file
					FileReference ResponseFileName = CompileAction.ProducedItems[0].Location + ".rsp";
					FileItem ResponseFileItem = Graph.CreateIntermediateTextFile(ResponseFileName, new List<string> { AllArguments });
					string ResponseArgument = string.Format("@\"{0}\"", ResponseFileName);

					CompileAction.WorkingDirectory = Unreal.EngineSourceDirectory;
					CompileAction.CommandDescription = IsAnalyzing(CompileEnvironment) ? "Analyze" : "Compile";
					if (bExecuteCompilerThroughShell)
					{
						SetupActionToExecuteCompilerThroughShell(ref CompileAction, ClangPath!, ResponseArgument, CompileAction.CommandDescription);
					}
					else
					{
						CompileAction.CommandPath = new FileReference(ClangPath!);
						CompileAction.CommandArguments = ResponseArgument;
					}
					CompileAction.PrerequisiteItems.Add(ResponseFileItem);
					CompileAction.CommandVersion = AndroidClangBuild!;

					CompileAction.StatusDescription = string.Format("{0} [{1}]", Path.GetFileName(SourceFile.AbsolutePath), Arch.Replace("-", ""));

					// VC++ always outputs the source file name being compiled, so we don't need to emit this ourselves
					CompileAction.bShouldOutputStatusDescription = true;

					// Don't farm out creation of pre-compiled headers as it is the critical path task.
					CompileAction.bCanExecuteRemotely =
						CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
						CompileEnvironment.bAllowRemotelyCompiledPCHs;
				}
			}

			return Result;
		}

		public override FileItem? LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			return null;
		}

		static public string InlineArchName(string Pathname, string Arch, bool bUseShortNames = false)
		{
			string FinalArch = Arch;
			if (bUseShortNames)
			{
				FinalArch = ShortArchNames[FinalArch];
			}
			return Path.Combine(Path.GetDirectoryName(Pathname)!, Path.GetFileNameWithoutExtension(Pathname) + FinalArch + Path.GetExtension(Pathname));
		}

		public string RemoveArchName(string Pathname)
		{
			// remove all architecture names
			foreach (string Arch in GetAllArchitectures())
			{
				Pathname = Path.Combine(Path.GetDirectoryName(Pathname)!, Path.GetFileName(Pathname).Replace(Arch, ""));
			}
			return Pathname;
		}

		static public DirectoryReference InlineArchIncludeFolder(DirectoryReference PathRef, string Arch)
		{
			return DirectoryReference.Combine(PathRef, "include", Arch.Replace("-", ""));
		}

		public override CPPOutput GenerateISPCHeaders(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, IActionGraphBuilder Graph)
		{
			if (Arches!.Count == 0)
			{
				throw new BuildException("At least one architecture (armv7, x86, etc) needs to be selected in the project settings to build");
			}

			CPPOutput Result = new CPPOutput();

			if (!CompileEnvironment.bCompileISPC)
			{
				return Result;
			}

			foreach (string Arch in Arches)
			{
				List<string> CompileTargets = GetISPCCompileTargets(CompileEnvironment.Platform, Arch);

				CompileEnvironment.UserIncludePaths.Add(InlineArchIncludeFolder(OutputDir, Arch));

				foreach (FileItem ISPCFile in InputFiles)
				{
					Action CompileAction = Graph.CreateAction(ActionType.Compile);
					CompileAction.CommandDescription = "Compile";
					CompileAction.WorkingDirectory = Unreal.EngineSourceDirectory;
					CompileAction.CommandPath = new FileReference(GetISPCHostCompilerPath(BuildHostPlatform.Current.Platform));
					CompileAction.StatusDescription = Path.GetFileName(ISPCFile.AbsolutePath);
					CompileAction.CommandVersion = GetISPCHostCompilerVersion(BuildHostPlatform.Current.Platform).ToString();

					// Disable remote execution to workaround mismatched case on XGE
					CompileAction.bCanExecuteRemotely = false;

					List<string> Arguments = new List<string>();

					// Add the ISPC obj file as a prerequisite of the action.
					Arguments.Add(String.Format(" \"{0}\"", ISPCFile.AbsolutePath));

					// Add the ISPC h file to the produced item list.
					FileItem ISPCIncludeHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							InlineArchIncludeFolder(OutputDir, Arch),
							Path.GetFileName(ISPCFile.AbsolutePath) + ".generated.dummy.h"
							)
						);

					// Add the ISPC file to be compiled.
					Arguments.Add(String.Format("-h \"{0}\"", ISPCIncludeHeaderFile));

					// Build target string. No comma on last
					string TargetString = "";
					foreach (string Target in CompileTargets)
					{
						if (Target == CompileTargets.Last())
						{
							TargetString += Target;
						}
						else
						{
							TargetString += Target + ",";
						}
					}

					// Build target triplet
					Arguments.Add(String.Format("--target-os=\"{0}\"", GetISPCOSTarget(CompileEnvironment.Platform)));
					Arguments.Add(String.Format("--arch=\"{0}\"", GetISPCArchTarget(CompileEnvironment.Platform, Arch)));
					Arguments.Add(String.Format("--target=\"{0}\"", TargetString));

					Arguments.Add("--pic");

					// Include paths. Don't use AddIncludePath() here, since it uses the full path and exceeds the max command line length.
					foreach (DirectoryReference IncludePath in CompileEnvironment.UserIncludePaths)
					{
						Arguments.Add(String.Format("-I\"{0}\"", IncludePath));
					}

					// System include paths.
					foreach (DirectoryReference SystemIncludePath in CompileEnvironment.SystemIncludePaths)
					{
						Arguments.Add(String.Format("-I\"{0}\"", SystemIncludePath));
					}

					// Generate the included header dependency list
					if (CompileEnvironment.bGenerateDependenciesFile)
					{
						FileItem DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, InlineArchName(Path.GetFileName(ISPCFile.AbsolutePath) + ".d", Arch, true)));
						Arguments.Add(String.Format("-M -MF \"{0}\"", DependencyListFile.AbsolutePath.Replace('\\', '/')));
						CompileAction.DependencyListFile = DependencyListFile;
						CompileAction.ProducedItems.Add(DependencyListFile);
					}

					CompileAction.ProducedItems.Add(ISPCIncludeHeaderFile);

					CompileAction.CommandArguments = String.Join(" ", Arguments);

					// Add the source file and its included files to the prerequisite item list.
					CompileAction.PrerequisiteItems.Add(ISPCFile);
					CompileAction.StatusDescription = string.Format("{0} [{1}]", Path.GetFileName(ISPCFile.AbsolutePath), Arch.Replace("-", ""));

					FileItem ISPCFinalHeaderFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							InlineArchIncludeFolder(OutputDir, Arch),
							Path.GetFileName(ISPCFile.AbsolutePath) + ".generated.h"
							)
						);

					// Fix interrupted build issue by copying header after generation completes
					FileReference SourceFile = ISPCIncludeHeaderFile.Location;
					FileReference TargetFile = ISPCFinalHeaderFile.Location;

					FileItem SourceFileItem = FileItem.GetItemByFileReference(SourceFile);
					FileItem TargetFileItem = FileItem.GetItemByFileReference(TargetFile);

					Action CopyAction = Graph.CreateAction(ActionType.BuildProject);
					CopyAction.CommandDescription = "Copy";
					CopyAction.CommandPath = BuildHostPlatform.Current.Shell;
					if (BuildHostPlatform.Current.ShellType == ShellType.Cmd)
					{
						CopyAction.CommandArguments = String.Format("/C \"copy /Y \"{0}\" \"{1}\" 1>nul\"", SourceFile, TargetFile);
					}
					else
					{
						CopyAction.CommandArguments = String.Format("-c 'cp -f \"{0}\" \"{1}\"'", SourceFile.FullName, TargetFile.FullName);
					}
					CopyAction.WorkingDirectory = Unreal.EngineSourceDirectory;
					CopyAction.PrerequisiteItems.Add(SourceFileItem);
					CopyAction.ProducedItems.Add(TargetFileItem);
					CopyAction.StatusDescription = TargetFileItem.Location.GetFileName();
					CopyAction.bCanExecuteRemotely = false;
					CopyAction.bShouldOutputStatusDescription = false;

					Result.GeneratedHeaderFiles.Add(TargetFileItem);

					Logger.LogDebug("   ISPC Generating Header {StatusDescription}: \"{CommandPath}\" {CommandArguments}", CompileAction.StatusDescription, CompileAction.CommandPath, CompileAction.CommandArguments);
				}
			}

			return Result;
		}
		
		public override CPPOutput CompileISPCFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, IActionGraphBuilder Graph)
		{
			if (Arches!.Count == 0)
			{
				throw new BuildException("At least one architecture (armv7, x86, etc) needs to be selected in the project settings to build");
			}

			CPPOutput Result = new CPPOutput();

			if (!CompileEnvironment.bCompileISPC)
			{
				return Result;
			}

			foreach (string Arch in Arches)
			{
				List<string> CompileTargets = GetISPCCompileTargets(CompileEnvironment.Platform, Arch);

				foreach (FileItem ISPCFile in InputFiles)
				{
					Action CompileAction = Graph.CreateAction(ActionType.Compile);
					CompileAction.CommandDescription = "Compile";
					CompileAction.WorkingDirectory = Unreal.EngineSourceDirectory;
					CompileAction.CommandPath = new FileReference(GetISPCHostCompilerPath(BuildHostPlatform.Current.Platform));
					CompileAction.StatusDescription = Path.GetFileName(ISPCFile.AbsolutePath);

					// Disable remote execution to workaround mismatched case on XGE
					CompileAction.bCanExecuteRemotely = false;

					List<string> Arguments = new List<string>();

					// Add the ISPC file to be compiled.
					Arguments.Add(String.Format(" \"{0}\"", ISPCFile.AbsolutePath));

					List<FileItem> CompiledISPCObjFiles = new List<FileItem>();
					List<FileItem> FinalISPCObjFiles = new List<FileItem>();
					string TargetString = "";

					foreach (string Target in CompileTargets)
					{
						string ObjTarget = Target;

						if (Target.Contains("-"))
						{
							// Remove lane width and gang size from obj file name
							ObjTarget = Target.Split('-')[0];
						}

						FileItem CompiledISPCObjFile;
						FileItem FinalISPCObjFile;

						if (CompileTargets.Count > 1)
						{
							CompiledISPCObjFile = FileItem.GetItemByFileReference(
							FileReference.Combine(
								OutputDir,
								Path.GetFileNameWithoutExtension(InlineArchName(Path.GetFileName(ISPCFile.AbsolutePath) + ".o", Arch, true)) + "_" + ObjTarget + ".o"
								)
							);

							FinalISPCObjFile = FileItem.GetItemByFileReference(
							FileReference.Combine(
								OutputDir,
								Path.GetFileName(ISPCFile.AbsolutePath) + "_" + ObjTarget + InlineArchName(".o", Arch, true)
								)
							);
						}
						else
						{
							CompiledISPCObjFile = FileItem.GetItemByFileReference(
								FileReference.Combine(
									OutputDir,
									InlineArchName(Path.GetFileName(ISPCFile.AbsolutePath) + ".o", Arch, true)
									)
								);

							FinalISPCObjFile = CompiledISPCObjFile;
						}

						// Add the ISA specific ISPC obj files to the produced item list.
						CompiledISPCObjFiles.Add(CompiledISPCObjFile);
						FinalISPCObjFiles.Add(FinalISPCObjFile);

						// Build target string. No comma on last
						if (Target == CompileTargets.Last())
						{
							TargetString += Target;
						}
						else
						{
							TargetString += Target + ",";
						}
					}

					// Add the common ISPC obj file to the produced item list.
					FileItem CompiledISPCObjFileNoISA = FileItem.GetItemByFileReference(
						FileReference.Combine(
							OutputDir,
							InlineArchName(Path.GetFileName(ISPCFile.AbsolutePath) + ".o", Arch, true)
							)
						);

					CompiledISPCObjFiles.Add(CompiledISPCObjFileNoISA);
					FinalISPCObjFiles.Add(CompiledISPCObjFileNoISA);

					// Add the output ISPC obj file
					Arguments.Add(String.Format("-o \"{0}\"", CompiledISPCObjFileNoISA));

					// Build target triplet
					Arguments.Add(String.Format("--target-os=\"{0}\"", GetISPCOSTarget(CompileEnvironment.Platform)));
					Arguments.Add(String.Format("--arch=\"{0}\"", GetISPCArchTarget(CompileEnvironment.Platform, Arch)));
					Arguments.Add(String.Format("--target=\"{0}\"", TargetString));

					if (CompileEnvironment.Configuration == CppConfiguration.Debug)
					{
						Arguments.Add("-g -O0");
					}
					else
					{
						Arguments.Add("-O2");
					}

					Arguments.Add("--pic");

					// Add include paths to the argument list (filtered by architecture)
					foreach (DirectoryReference IncludePath in CompileEnvironment.SystemIncludePaths)
					{
						if (IsDirectoryForArch(IncludePath.FullName, Arch))
						{
							Arguments.Add(string.Format(" -I\"{0}\"", IncludePath));
						}
					}
					foreach (DirectoryReference IncludePath in CompileEnvironment.UserIncludePaths)
					{
						if (IsDirectoryForArch(IncludePath.FullName, Arch))
						{
							Arguments.Add(string.Format(" -I\"{0}\"", IncludePath));
						}
					}

					// Preprocessor definitions.
					foreach (string Definition in CompileEnvironment.Definitions)
					{
						Arguments.Add(String.Format(" -D\"{0}\"", Definition));
					}

					// Consume the included header dependency list
					if (CompileEnvironment.bGenerateDependenciesFile)
					{
						FileItem DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, InlineArchName(Path.GetFileName(ISPCFile.AbsolutePath) + ".d", Arch, true)));
						CompileAction.DependencyListFile = DependencyListFile;
						CompileAction.PrerequisiteItems.Add(DependencyListFile);
					}

					CompileAction.ProducedItems.AddRange(CompiledISPCObjFiles);

					CompileAction.CommandArguments = String.Join(" ", Arguments);

					// Add the source file and its included files to the prerequisite item list.
					CompileAction.PrerequisiteItems.Add(ISPCFile);

					CompileAction.StatusDescription = string.Format("{0} [{1}]", Path.GetFileName(ISPCFile.AbsolutePath), Arch.Replace("-", ""));

					for(int i = 0; i < CompiledISPCObjFiles.Count; i++)
					{
						// ISPC compiler can't add suffix on the end of the arch, so copy to put into what linker expects
						FileReference SourceFile = CompiledISPCObjFiles[i].Location;
						FileReference TargetFile = FinalISPCObjFiles[i].Location;

						if (SourceFile.Equals(TargetFile))
						{
							continue;
						}

						FileItem SourceFileItem = FileItem.GetItemByFileReference(SourceFile);
						FileItem TargetFileItem = FileItem.GetItemByFileReference(TargetFile);

						Action CopyAction = Graph.CreateAction(ActionType.BuildProject);
						CopyAction.CommandDescription = "Copy";
						CopyAction.CommandPath = BuildHostPlatform.Current.Shell;
						if (BuildHostPlatform.Current.ShellType == ShellType.Cmd)
						{
							CopyAction.CommandArguments = String.Format("/C \"copy /Y \"{0}\" \"{1}\" 1>nul\"", SourceFile, TargetFile);
						}
						else
						{
							CopyAction.CommandArguments = String.Format("-c 'cp -f \"{0}\" \"{1}\"'", SourceFile.FullName, TargetFile.FullName);
						}
						CopyAction.WorkingDirectory = Unreal.EngineSourceDirectory;
						CopyAction.PrerequisiteItems.Add(SourceFileItem);
						CopyAction.ProducedItems.Add(TargetFileItem);
						CopyAction.StatusDescription = TargetFileItem.Location.GetFileName();
						CopyAction.bCanExecuteRemotely = false;
						CopyAction.bShouldOutputStatusDescription = false;
					}

					Result.ObjectFiles.AddRange(FinalISPCObjFiles);

					Logger.LogDebug("   ISPC Compiling {StatusDescription}: \"{CommandPath}\" {CommandArguments}", CompileAction.StatusDescription, CompileAction.CommandPath, CompileAction.CommandArguments);
				}
			}

			return Result;
		}

		public override FileItem[] LinkAllFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			List<FileItem> Outputs = new List<FileItem>();

			if (!LinkEnvironment.bIsBuildingLibrary)
			{
				// @todo Lumin: will this add them multiple times?
				ModifyLibraries(LinkEnvironment);
			}

			for (int ArchIndex = 0; ArchIndex < Arches!.Count; ArchIndex++)
			{
				string Arch = Arches[ArchIndex];

				int OutputPathIndex = ArchIndex;

				// Android will have an array of outputs
				if (!LinkEnvironment.bIsBuildingDLL && // DLL compiles don't have the Arch embedded in the name
					(LinkEnvironment.OutputFilePaths.Count < OutputPathIndex ||
					!LinkEnvironment.OutputFilePaths[OutputPathIndex].GetFileNameWithoutExtension().EndsWith(Arch)))
				{
					throw new BuildException("The OutputFilePaths array didn't match the Arches array in AndroidToolChain.LinkAllFiles");
				}

				// Create an action that invokes the linker.
				Action LinkAction = Graph.CreateAction(ActionType.Link);
				LinkAction.WorkingDirectory = Unreal.EngineSourceDirectory;

				if (LinkEnvironment.bIsBuildingLibrary)
				{
					switch (Arch)
					{
						case "-arm64": LinkAction.CommandPath = new FileReference(ArPathArm64!); break;
						case "-x64": LinkAction.CommandPath = new FileReference(ArPathx64!); break;
					}
				}
				else
				{
					LinkAction.CommandPath = new FileReference(ClangPath!);
				}

				DirectoryReference LinkerPath = LinkAction.WorkingDirectory;

				LinkAction.WorkingDirectory = LinkEnvironment.IntermediateDirectory!;

				// Get link arguments.
				LinkAction.CommandArguments = LinkEnvironment.bIsBuildingLibrary ? GetArArguments(LinkEnvironment) : GetLinkArguments(LinkEnvironment, Arch);

				// Add the output file as a production of the link action.
				FileItem OutputFile;
				OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePaths[OutputPathIndex]);
				Outputs.Add(OutputFile);
				LinkAction.ProducedItems.Add(OutputFile);
				LinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputFile.AbsolutePath));
				LinkAction.CommandVersion = AndroidClangBuild!;

				// LinkAction.bPrintDebugInfo = true;

				// Add the output file to the command-line.
				if (LinkEnvironment.bIsBuildingLibrary)
				{
					LinkAction.CommandArguments += string.Format(" \"{0}\"", OutputFile.AbsolutePath);
				}
				else
				{
					LinkAction.CommandArguments += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);
				}

				// Add the input files to a response file, and pass the response file on the command-line.
				List<string> InputFileNames = new List<string>();
				foreach (FileItem InputFile in LinkEnvironment.InputFiles)
				{
					// make sure it's for current Arch
					if (Path.GetFileNameWithoutExtension(InputFile.AbsolutePath).EndsWith(ShortArchNames[Arch]))
					{
						string InputPath;
						if (InputFile.Location.IsUnderDirectory(LinkEnvironment.IntermediateDirectory!))
						{
							InputPath = InputFile.Location.MakeRelativeTo(LinkEnvironment.IntermediateDirectory!);
						}
						else
						{
							InputPath = InputFile.Location.FullName;
						}
						InputFileNames.Add(string.Format("\"{0}\"", InputPath.Replace('\\', '/')));

						LinkAction.PrerequisiteItems.Add(InputFile);
					}
				}

				string LinkResponseArguments = "";

				// libs don't link in other libs
				if (!LinkEnvironment.bIsBuildingLibrary)
				{
					// Make a list of library paths to search
					List<string> AdditionalLibraryPaths = new List<string>();
					List<string> AdditionalLibraries = new List<string>();

					// Add the library paths to the additional path list
					foreach (DirectoryReference LibraryPath in LinkEnvironment.SystemLibraryPaths)
					{
						// LinkerPaths could be relative or absolute
						string AbsoluteLibraryPath = Utils.ExpandVariables(LibraryPath.FullName);
						if (IsDirectoryForArch(AbsoluteLibraryPath, Arch))
						{
							// environment variables aren't expanded when using the $( style
							if (Path.IsPathRooted(AbsoluteLibraryPath) == false)
							{
								AbsoluteLibraryPath = Path.Combine(LinkerPath.FullName, AbsoluteLibraryPath);
							}
							AbsoluteLibraryPath = Utils.CollapseRelativeDirectories(AbsoluteLibraryPath);
							if (!AdditionalLibraryPaths.Contains(AbsoluteLibraryPath))
							{
								AdditionalLibraryPaths.Add(AbsoluteLibraryPath);
							}
						}
					}

					// discover additional libraries and their paths
					foreach (string SystemLibrary in LinkEnvironment.SystemLibraries)
					{
						if (!ShouldSkipLib(SystemLibrary, Arch))
						{
							if (String.IsNullOrEmpty(Path.GetDirectoryName(SystemLibrary)))
							{
								if (SystemLibrary.StartsWith("lib"))
								{
									AdditionalLibraries.Add(SystemLibrary);
								}
								else
								{
									AdditionalLibraries.Add("lib" + SystemLibrary);
								}
							}
						}
					}
					foreach (FileReference Library in LinkEnvironment.Libraries)
					{
						if (!ShouldSkipLib(Library.FullName, Arch))
						{
							string AbsoluteLibraryPath = Path.GetDirectoryName(Library.FullName)!;
							LinkAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(Library));

							string Lib = Path.GetFileNameWithoutExtension(Library.FullName);
							if (Lib.StartsWith("lib"))
							{
								AdditionalLibraries.Add(Lib);
								if (!AdditionalLibraryPaths.Contains(AbsoluteLibraryPath))
								{
									AdditionalLibraryPaths.Add(AbsoluteLibraryPath);
								}
							}
							else
							{
								AdditionalLibraries.Add(Library.FullName);
							}

							if (!DisableLibCppSharedDependencyValidation() && ReadElfPath != null)
							{
								string? Output = Utils.RunLocalProcessAndReturnStdOut(ReadElfPath, "--dynamic \"" + Library.FullName + "\"");
								if (Output != null)
								{
									if (Output.Contains("libc++_shared.so"))
									{
										if (IsNewNDKModel())
										{
											throw new BuildException("Lib {0} depends on libc++_shared.so. There are known incompatibility issues when linking libc++_shared.so with Unreal Engine built with NDK22+." +
												" Please rebuild your dependencies with static libc++!", Lib);
										}
										else
										{
											Log.TraceWarning("Lib {0} depends on libc++_shared.so. Unreal Engine is designed to be linked with libs that are built against static libc++ only. Please rebuild your dependencies with static libc++!", Lib);
										}
									}
								}
							}
						}
					}

					// add the library paths to response
					foreach (string LibaryPath in AdditionalLibraryPaths)
					{
						LinkResponseArguments += string.Format(" -L\"{0}\"", LibaryPath);
					}

					// add libraries in a library group
					LinkResponseArguments += string.Format(" -Wl,--start-group");
					foreach (string AdditionalLibrary in AdditionalLibraries)
					{
						if (AdditionalLibrary.StartsWith("lib"))
						{
							LinkResponseArguments += string.Format(" \"-l{0}\"", AdditionalLibrary.Substring(3));
						}
						else
						{
							LinkResponseArguments += string.Format(" \"{0}\"", AdditionalLibrary);
						}
					}
					LinkResponseArguments += string.Format(" -Wl,--end-group");

					// Write the MAP file to the output directory.
					if (LinkEnvironment.bCreateMapFile)
					{
						FileReference MAPFilePath = FileReference.Combine(LinkEnvironment.OutputDirectory!, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".map");
						FileItem MAPFile = FileItem.GetItemByFileReference(MAPFilePath);
						LinkResponseArguments += String.Format(" -Wl,--cref -Wl,-Map,\"{0}\"", MAPFilePath);
						LinkAction.ProducedItems.Add(MAPFile);

						// Export a list of object file paths, so we can locate the object files referenced by the map file
						ExportObjectFilePaths(LinkEnvironment, Path.ChangeExtension(MAPFilePath.FullName, ".objpaths"));
					}
				}

				// Add the additional arguments specified by the environment.
				LinkResponseArguments += LinkEnvironment.AdditionalArguments;

				// Write out a response file
				FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);
				InputFileNames.Add(LinkResponseArguments.Replace("\\", "/"));

				FileItem ResponseFileItem = Graph.CreateIntermediateTextFile(ResponseFileName, InputFileNames);

				LinkAction.CommandArguments += string.Format(" @\"{0}\"", ResponseFileName);
				LinkAction.PrerequisiteItems.Add(ResponseFileItem);

				// Fix up the paths in commandline
				LinkAction.CommandArguments = LinkAction.CommandArguments.Replace("\\", "/");

				// Only execute linking on the local PC.
				LinkAction.bCanExecuteRemotely = false;

				string VersionScriptFileItem = GetVersionScriptFilename(LinkEnvironment);
				LinkAction.PrerequisiteItems.Add(FileItem.GetItemByPath(VersionScriptFileItem));
				
				if(bExecuteCompilerThroughShell)
				{
					SetupActionToExecuteCompilerThroughShell(ref LinkAction, LinkAction.CommandPath.FullName, LinkAction.CommandArguments, "Link");
				}

				//Logger.LogInformation("Link: {LinkActionCommandPathFullName} {LinkActionCommandArguments}", LinkAction.CommandPath.FullName, LinkAction.CommandArguments);

				// Windows can run into an issue with too long of a commandline when clang tries to call ld to link.
				// To work around this we call clang to just get the command it would execute and generate a
				// second response file to directly call ld with the right arguments instead of calling through clang.
/* disable while tracking down some linker errors this introduces
				if (RuntimePlatform.IsWindows)
				{
					// capture the actual link command without running it
					ProcessStartInfo StartInfo = new ProcessStartInfo();
					StartInfo.WorkingDirectory = LinkEnvironment.IntermediateDirectory.FullName;
					StartInfo.FileName = LinkAction.CommandPath;
					StartInfo.Arguments = "-### " + LinkAction.CommandArguments;
					StartInfo.UseShellExecute = false;
					StartInfo.CreateNoWindow = true;
					StartInfo.RedirectStandardError = true;

					LinkerCommandline = "";

					Process Proc = new Process();
					Proc.StartInfo = StartInfo;
					Proc.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedForLinker);
					Proc.Start();
					Proc.BeginErrorReadLine();
					Proc.WaitForExit(5000);

					LinkerCommandline = LinkerCommandline.Trim();

					// the command should be in quotes; if not we'll just use clang to link as usual
					int FirstQuoteIndex = LinkerCommandline.IndexOf('"');
					if (FirstQuoteIndex >= 0)
					{
						int SecondQuoteIndex = LinkerCommandline.Substring(FirstQuoteIndex + 1).IndexOf('"');
						if (SecondQuoteIndex >= 0)
						{
							LinkAction.CommandPath = LinkerCommandline.Substring(FirstQuoteIndex + 1, SecondQuoteIndex - FirstQuoteIndex);
							LinkAction.CommandArguments = LinkerCommandline.Substring(FirstQuoteIndex + SecondQuoteIndex + 3);

							// replace double backslashes
							LinkAction.CommandPath = LinkAction.CommandPath.Replace("\\\\", "/");

							// now create a response file for the full command using ld directly
							FileReference FinalResponseFileName = FileReference.Combine(LinkEnvironment.IntermediateDirectory, OutputFile.Location.GetFileName() + ".responseFinal");
							FileItem FinalResponseFileItem = Graph.CreateIntermediateTextFile(FinalResponseFileName, LinkAction.CommandArguments);
							LinkAction.CommandArguments = string.Format("@\"{0}\"", FinalResponseFileName);
							LinkAction.PrerequisiteItems.Add(FinalResponseFileItem);
						}
					}
				}
*/
			}

			return Outputs.ToArray();
		}

		// captures stderr from clang
		private static string LinkerCommandline = "";
		static public void OutputReceivedForLinker(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null) && (Line.Data.Contains("--sysroot")))
			{
				LinkerCommandline += Line.Data;
			}
		}

		private void ExportObjectFilePaths(LinkEnvironment LinkEnvironment, string FileName)
		{
			// Write the list of object file directories
			HashSet<DirectoryReference> ObjectFileDirectories = new HashSet<DirectoryReference>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				ObjectFileDirectories.Add(InputFile.Location.Directory);
			}
			foreach (FileReference Library in LinkEnvironment.Libraries)
			{
				ObjectFileDirectories.Add(Library.Directory);
			}
			foreach (DirectoryReference LibraryPath in LinkEnvironment.SystemLibraryPaths)
			{
				ObjectFileDirectories.Add(LibraryPath);
			}
			foreach (string LibraryPath in (Environment.GetEnvironmentVariable("LIB") ?? "").Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries))
			{
				ObjectFileDirectories.Add(new DirectoryReference(LibraryPath));
			}
			Directory.CreateDirectory(Path.GetDirectoryName(FileName)!);
			File.WriteAllLines(FileName, ObjectFileDirectories.Select(x => x.FullName).OrderBy(x => x).ToArray());
		}

		public override void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
			// only the .so needs to be in the manifest; we always have to build the apk since its contents depend on the project

			/*
			// the binary will have all of the .so's in the output files, we need to trim down to the shared apk (which is what needs to go into the manifest)
			if (Target.bDeployAfterCompile && Binary.Config.Type != UEBuildBinaryType.StaticLibrary)
			{
				foreach (FileReference BinaryPath in Binary.Config.OutputFilePaths)
				{
					FileReference ApkFile = BinaryPath.ChangeExtension(".apk");
					BuildProducts.Add(ApkFile, BuildProductType.Package);
				}
			}
			*/
		}

		public static void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line, ILogger Logger)
		{
			if ((Line != null) && (Line.Data != null))
			{
				Logger.LogInformation("{Output}", Line.Data);
			}
		}

		public virtual string GetStripPath(FileReference SourceFile)
		{
			string StripExe;
			if (SourceFile.FullName.Contains("-arm64"))
			{
				StripExe = ArPathArm64!;
			}
			else
			if (SourceFile.FullName.Contains("-x64"))
			{
				StripExe = ArPathx64!;
			}
			else
			{
				throw new BuildException("Couldn't determine Android architecture to strip symbols from {0}", SourceFile.FullName);
			}

			// fix the executable (replace the last -ar with -strip and keep any extension)
			int ArIndex = StripExe.LastIndexOf("-ar");
			StripExe = StripExe.Substring(0, ArIndex) + "-strip" + StripExe.Substring(ArIndex + 3);
			return StripExe;
		}

		public void StripSymbols(FileReference SourceFile, FileReference TargetFile, ILogger Logger)
		{
			if (SourceFile != TargetFile)
			{
				// Strip command only works in place so we need to copy original if target is different
				File.Copy(SourceFile.FullName, TargetFile.FullName, true);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = GetStripPath(SourceFile).Trim('"');
			StartInfo.Arguments = " --strip-debug \"" + TargetFile.FullName + "\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo, Logger);
		}

		protected virtual void SetupActionToExecuteCompilerThroughShell(ref Action CompileOrLinkAction, string CommandPath, string CommandArguments, string CommandDescription)
		{
			string QuotedCommandPath = CommandPath;
			if (CommandPath.Contains(' '))
			{
				QuotedCommandPath = "'" + CommandPath + "'";
			}
	
			if (BuildHostPlatform.Current.ShellType == ShellType.Cmd)
			{
				CompileOrLinkAction.CommandArguments = String.Format("/c \"{0} {1}\"", QuotedCommandPath, CommandArguments);
			}
			else
			{
				CompileOrLinkAction.CommandArguments = String.Format("-c \'{0} {1}\'", QuotedCommandPath, CommandArguments);
			}

			CompileOrLinkAction.CommandPath = BuildHostPlatform.Current.Shell;
			CompileOrLinkAction.CommandDescription = CommandDescription;
		}

		public ClangSanitizer BuildWithSanitizer()
		{
			if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer))
			{
				return ClangSanitizer.Address;
			}
			else if (Options.HasFlag(ClangToolChainOptions.EnableHWAddressSanitizer))
			{
				return ClangSanitizer.HwAddress;
			}
			else if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
			{
				return ClangSanitizer.Thread;
			}
			else if (Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer))
			{
				return ClangSanitizer.UndefinedBehavior;
			}
			else if (Options.HasFlag(ClangToolChainOptions.EnableMinimalUndefinedBehaviorSanitizer))
			{
				return ClangSanitizer.UndefinedBehaviorMinimal;
			}

			return ClangSanitizer.None;
		}
	};
}
