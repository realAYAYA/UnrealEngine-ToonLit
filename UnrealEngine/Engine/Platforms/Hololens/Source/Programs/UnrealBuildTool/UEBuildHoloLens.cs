// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using EpicGames.Core;
using System.Text.RegularExpressions;
using System.Runtime.Versioning;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	partial struct UnrealPlatformGroup
	{
		/// <summary>
		/// this group was for grouping HoloLens32 and HoloLens64 into HoloLens directories but is no longer relevant
		/// </summary>
		[Obsolete("UnrealPlatformGroup.HoloLens is deprecated - please change the code to use ModuleName_HoloLens.build.cs etc or reference the UnrealTargetPlatform.HoloLens directly")]
		public static UnrealPlatformGroup HoloLens = FindOrAddByName("HoloLens");
	}

	/// <summary>
	/// HoloLens-specific target settings
	/// </summary>
	public class HoloLensTargetRules
	{
		/// <summary>
		/// Version of the compiler toolchain to use on HoloLens. A value of "default" will be changed to a specific version at UBT startup.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "CompilerVersion")]
		[XmlConfigFile(Category = "HoloLensPlatform")]
		[CommandLine("-2019", Value = nameof(WindowsCompiler.VisualStudio2019))]
		[CommandLine("-2022", Value = nameof(WindowsCompiler.VisualStudio2022))]
		[CommandLine("-Compiler=")]
		public WindowsCompiler Compiler = WindowsCompiler.Default;

		/// <summary>
		/// Enable PIX debugging (automatically disabled in Shipping and Test configs)
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "bEnablePIXProfiling")]
		public bool bPixProfilingEnabled = true;

		/// <summary>
		/// Version of the compiler toolchain to use on HoloLens. A value of "default" will be changed to a specific version at UBT startup.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "bBuildForRetailWindowsStore")]
		public bool bBuildForRetailWindowsStore = false;

		/// <summary>
		/// Contains the specific version of the Windows 10 SDK that we will build against. If empty, it will be "Latest"
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "Windows10SDKVersion")]
		public string? Win10SDKVersionString = null;

		internal Version? Win10SDKVersion = null;

        /// <summary>
        /// Automatically increment the project version after each build.
        /// </summary>
        [ConfigFile(ConfigHierarchyType.Engine, "/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "bAutoIncrementVersion")]
        public bool bAutoIncrementVersion = false;

		/// <summary>
		/// Whether to run native code analysis at compile time, producing a nativecodeanalysis.xml file for every compiled source file
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "bRunNativeCodeAnalysis")]
		public bool bRunNativeCodeAnalysis = false;
		/// <summary>
		/// A project relative path for a custom native code analysis ruleset xml file
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/HoloLensPlatformEditor.HoloLensTargetSettings", "NativeCodeAnalysisRuleset")]
		public string? NativeCodeAnalysisRuleset = null;
	}

	/// <summary>
	/// Read-only wrapper for HoloLens-specific target settings
	/// </summary>
	public class ReadOnlyHoloLensTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private HoloLensTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyHoloLensTargetRules(HoloLensTargetRules Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>
		#region Read-only accessor properties 
#pragma warning disable CS1591
		public WindowsCompiler Compiler
		{
			get { return Inner.Compiler; }
		}

		public bool bPixProfilingEnabled
		{
			get { return Inner.bPixProfilingEnabled; }
		}

		public bool bBuildForRetailWindowsStore
		{
			get { return Inner.bBuildForRetailWindowsStore; }
		}

		public Version? Win10SDKVersion
		{
			get { return Inner.Win10SDKVersion; }
		}
		public string? Win10SDKVersionString
		{
			get { return Inner.Win10SDKVersionString; }
		}
		
		public bool bRunNativeCodeAnalysis
		{
			get { return Inner.bRunNativeCodeAnalysis; }
		}
		public string? NativeCodeAnalysisRuleset
		{
			get { return Inner.NativeCodeAnalysisRuleset; }
		}
#pragma warning restore CS1591
		#endregion
	}


	abstract partial class TargetRules
	{
		/// <summary>
		/// HoloLens-specific target settings.
		/// </summary>
		[ConfigSubObject]
		public HoloLensTargetRules HoloLensPlatform = new HoloLensTargetRules();

	}


	public partial class ReadOnlyTargetRules
	{
		private ReadOnlyHoloLensTargetRules? _HoloLensPlatform = null;
		/// <summary>
		/// Singleton accessor for HoloLensPlatform
		/// </summary>
		public ReadOnlyHoloLensTargetRules HoloLensPlatform
		{
			get
			{
				if (_HoloLensPlatform == null)
				{
					_HoloLensPlatform = new ReadOnlyHoloLensTargetRules(Inner.HoloLensPlatform);
				}
				return _HoloLensPlatform;
			}
		}
	}

	[SupportedOSPlatform("windows")]
	class HoloLensEnvironment : VCEnvironment
	{
		internal HoloLensEnvironment(VCEnvironmentParameters Params, ILogger Logger)
			: base(Params, Logger)
		{
		}

		/// <summary>
		/// Return the standard Visual C++ library path for the given platform in this toolchain
		/// </summary>
		protected override DirectoryReference GetToolChainLibsDir(UnrealTargetPlatform Platform)
		{
			string ArchFolder = WindowsExports.GetArchitectureSubpath(Architecture);

			// Add the standard Visual C++ library paths
			if (ToolChain.IsMSVC())
			{
				return DirectoryReference.Combine(ToolChainDir, "lib", ArchFolder, "store");
			}
			else
			{
				DirectoryReference LibsPath = DirectoryReference.Combine(ToolChainDir, "LIB", "store");

				if (Architecture == WindowsArchitecture.x64)
				{
					LibsPath = DirectoryReference.Combine(LibsPath, "amd64");
				}

				return LibsPath;
			}
		}
	};


	[SupportedOSPlatform("windows")]
	class HoloLensPlatform : WindowsPlatform
	{
		public static readonly Version MinimumSDKVersionRecommended = new Version(10, 0, 17763, 0);
		public static readonly Version MaximumSDKVersionTested = new Version(10, 0, 18362, int.MaxValue);

		public HoloLensPlatform(MicrosoftPlatformSDK InSDK, ILogger InLogger) 
			: base(UnrealTargetPlatform.HoloLens, InSDK, InLogger)
		{
		}

		protected override VCEnvironment CreateVCEnvironment(TargetRules Target)
		{
			VCEnvironmentParameters Params = new VCEnvironmentParameters(Target.WindowsPlatform.Compiler, Target.WindowsPlatform.ToolChain, Platform, Target.WindowsPlatform.Architecture, Target.WindowsPlatform.CompilerVersion, Target.WindowsPlatform.WindowsSdkVersion, null, Target.WindowsPlatform.bUseCPPWinRT, Logger);
			return new HoloLensEnvironment(Params, Logger);
		}

		public override void ValidateTarget(TargetRules Target)
		{
			// WindowsTargetRules are reused for HoloLens, so that build modules can keep the model that reuses "windows" configs for most cases
			// That means overriding those settings here that need to be adjusted for HoloLens

			// Compiler version and pix flags must be reloaded from the HoloLens hive

			// Currently BP-only projects don't load build-related settings from their remote ini when building UnrealGame.exe
			// (see TargetRules.cs, where the possibly-null project directory is passed to ConfigCache.ReadSettings).
			// It's important for HoloLens that we *do* use the project-specific settings when building (VS 2017 vs 2015 and
			// retail Windows Store are both examples).  Possibly this should be done on all platforms?  But in the interest
			// of not changing behavior on other platforms I'm limiting the scope.

			DirectoryReference? IniDirRef = DirectoryReference.FromFile(Target.ProjectFile);
			if (IniDirRef == null && !string.IsNullOrEmpty(UnrealBuildTool.GetRemoteIniPath()))
			{
				IniDirRef = new DirectoryReference(UnrealBuildTool.GetRemoteIniPath()!);
			}

			// Stash the current compiler choice (accounts for command line) in case ReadSettings reverts it to default
			WindowsCompiler CompilerBeforeReadSettings = Target.HoloLensPlatform.Compiler;

			ConfigCache.ReadSettings(IniDirRef, Platform, Target.HoloLensPlatform);

			WindowsArchitecture Architecture = WindowsArchitecture.x64;
			if( !String.IsNullOrEmpty(Target.Architecture))
			{
				Architecture = (WindowsArchitecture)Enum.Parse(typeof(WindowsArchitecture), Target.Architecture, true);
			}

			if (Target.HoloLensPlatform.Compiler == WindowsCompiler.Default)
			{
				if (CompilerBeforeReadSettings != WindowsCompiler.Default)
				{
					// Previous setting was more specific, use that
					Target.HoloLensPlatform.Compiler = CompilerBeforeReadSettings;
				}
				else
				{
					Target.HoloLensPlatform.Compiler = WindowsPlatform.GetDefaultCompiler(Target.ProjectFile, Architecture, Logger);
				}
			}

			if (Target.HoloLensPlatform.Compiler.IsClang())
			{
				Logger.LogWarning("Buiding HoloLens with {ClangToolchain} is not supported. The default compiler will be used.", Target.HoloLensPlatform.Compiler);
				Target.HoloLensPlatform.Compiler = WindowsPlatform.GetDefaultCompiler(Target.ProjectFile, Architecture, Logger);
			}

			if (!Target.bGenerateProjectFiles)
			{
				Log.TraceInformationOnce("Using {0} architecture for deploying to HoloLens device", Architecture);
			}

			Target.WindowsPlatform.Compiler = Target.HoloLensPlatform.Compiler;
			Target.WindowsPlatform.Architecture = Architecture;
			Target.WindowsPlatform.bPixProfilingEnabled = Target.HoloLensPlatform.bPixProfilingEnabled;
			Target.WindowsPlatform.bUseWindowsSDK10 = true;
			Target.WindowsPlatform.bStrictConformanceMode = false;

			Target.bDeployAfterCompile = true;

			// Disable Simplygon support if compiling against the NULL RHI.
			if (Target.GlobalDefinitions.Contains("USE_NULL_RHI=1"))
			{
				Target.bCompileSpeedTree = false;
			}

			// Use shipping binaries to avoid dependency on nvToolsExt which fails WACK.
			if (Target.Configuration == UnrealTargetConfiguration.Shipping)
			{
				Target.bUseShippingPhysXLibraries = true;
			}

			// Be resilient to SDKs being uninstalled but still referenced in the INI file
			VersionNumber? SelectedWindowsSdkVersion;
			DirectoryReference? SelectedWindowsSdkDir;
			if (WindowsPlatform.TryGetWindowsSdkDir(Target.HoloLensPlatform.Win10SDKVersionString, Logger, out SelectedWindowsSdkVersion, out SelectedWindowsSdkDir))
			{
				Target.WindowsPlatform.WindowsSdkVersion = Target.HoloLensPlatform.Win10SDKVersionString;
			}

			// set the correct architecture
			if (Target.Architecture.ToLower() == "arm64")
			{
				Target.WindowsPlatform.Architecture = WindowsArchitecture.ARM64;
			}			

			base.ValidateTarget(Target);

			// Windows 10 SDK version
			// Auto-detect latest compatible by default (recommended), allow for explicit override if necessary
			// Validate that the SDK isn't too old, and that the combination of VS and SDK is supported.

			Target.HoloLensPlatform.Win10SDKVersion = new Version(Target.WindowsPlatform.Environment!.WindowsSdkVersion.ToString());

			if(!Target.bGenerateProjectFiles)
			{
				Log.TraceInformationOnce("Building using Windows SDK version {0} for HoloLens and compiler {1} {2}", Target.HoloLensPlatform.Win10SDKVersion, Target.HoloLensPlatform.Compiler, Target.WindowsPlatform.CompilerVersion);

				if (Target.HoloLensPlatform.Win10SDKVersion < MinimumSDKVersionRecommended)
				{
					Logger.LogWarning("Your Windows SDK version {SdkVer} is older than the minimum recommended version ({RecommendedVer}) for HoloLens.  Consider upgrading.", Target.HoloLensPlatform.Win10SDKVersion, MinimumSDKVersionRecommended);
				}
				else if (Target.HoloLensPlatform.Win10SDKVersion > MaximumSDKVersionTested)
				{
					Logger.LogInformation("Your Windows SDK version ({SdkVer}) for HoloLens is newer than the highest tested with this version of UBT ({MaxTestedVer}).  This is probably fine, but if you encounter issues consider using an earlier SDK.", Target.HoloLensPlatform.Win10SDKVersion, MaximumSDKVersionTested);
				}
			}

			HoloLensExports.InitWindowsSdkToolPath(Target.HoloLensPlatform.Win10SDKVersion.ToString(), Logger);

			// ISPC currently doesn't support Windows-AArch64
			Target.bCompileISPC = false;
		}

		/// <summary>
		/// Gets the default HoloLens architecture
		/// </summary>
		/// <param name="ProjectFile">The project being built</param>
		/// <returns>The default architecture</returns>
		public override string GetDefaultArchitecture(FileReference? ProjectFile)
		{
			return WindowsArchitecture.x64.ToString();
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binrary type being built</param>
		/// <returns>string	The binary extenstion (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.Object:
					return ".obj";
				case UEBuildBinaryType.PrecompiledHeader:
					return ".pch";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		internal static DirectoryReference? GetCppCXMetadataLocation(WindowsCompiler Compiler, string CompilerVersion, WindowsArchitecture Architecture, ILogger Logger)
		{
			VersionNumber? SelectedToolChainVersion;
			DirectoryReference? SelectedToolChainDir;
			DirectoryReference? SelectedRedistDir;
			if (!WindowsPlatform.TryGetToolChainDir(Compiler, CompilerVersion, Architecture, Logger, out SelectedToolChainVersion, out SelectedToolChainDir, out SelectedRedistDir))
			{
				return null;
			}

			return GetCppCXMetadataLocation(Compiler, SelectedToolChainDir);
		}

		public static DirectoryReference? GetCppCXMetadataLocation(WindowsCompiler Compiler, DirectoryReference SelectedToolChainDir)
		{
			if (Compiler.IsMSVC())
			{
				return DirectoryReference.Combine(SelectedToolChainDir, "lib", "x86", "Store", "references");
			}
			else
			{
				return null;
			}
		}


		private static Version FindLatestVersionDirectory(string InDirectory, Version? NoLaterThan)
		{
			Version LatestVersion = new Version(0, 0, 0, 0);
			if (Directory.Exists(InDirectory))
			{
				string[] VersionDirectories = Directory.GetDirectories(InDirectory);
				foreach (string Dir in VersionDirectories)
				{
					string VersionString = Path.GetFileName(Dir);
					Version? FoundVersion;
					if (Version.TryParse(VersionString, out FoundVersion) && FoundVersion > LatestVersion)
					{
						if (NoLaterThan == null || FoundVersion <= NoLaterThan)
						{
							LatestVersion = FoundVersion;
						}
					}
				}
			}
			return LatestVersion;
		}

		internal static string GetLatestMetadataPathForApiContract(string ApiContract, WindowsCompiler Compiler, ILogger Logger)
		{
			DirectoryReference? SDKFolder;
			VersionNumber? SDKVersion;
			if (!WindowsPlatform.TryGetWindowsSdkDir("Latest", Logger, out SDKVersion, out SDKFolder))
			{
				return string.Empty;
			}

			DirectoryReference ReferenceDir = DirectoryReference.Combine(SDKFolder, "References");
			if (DirectoryReference.Exists(ReferenceDir))
			{
				// Prefer a contract from a suitable SDK-versioned subdir of the references folder when available (starts with 15063 SDK)
				DirectoryReference SDKVersionedReferenceDir = DirectoryReference.Combine(ReferenceDir, SDKVersion.ToString());
				DirectoryReference ContractDir = DirectoryReference.Combine(SDKVersionedReferenceDir, ApiContract);
				Version? ContractLatestVersion = null;
				FileReference? MetadataFileRef = null;
				if (DirectoryReference.Exists(ContractDir))
				{
					// Note: contract versions don't line up with Windows SDK versions (they're numbered independently as 1.0.0.0, 2.0.0.0, etc.)
					ContractLatestVersion = FindLatestVersionDirectory(ContractDir.FullName, null);
					MetadataFileRef = FileReference.Combine(ContractDir, ContractLatestVersion.ToString(), ApiContract + ".winmd");
				}

				// Retry in unversioned references dir if we failed above.
				if (MetadataFileRef == null || !FileReference.Exists(MetadataFileRef))
				{
					ContractDir = DirectoryReference.Combine(ReferenceDir, ApiContract);
					if (DirectoryReference.Exists(ContractDir))
					{
						ContractLatestVersion = FindLatestVersionDirectory(ContractDir.FullName, null);
						MetadataFileRef = FileReference.Combine(ContractDir, ContractLatestVersion.ToString(), ApiContract + ".winmd");
					}
				}
				if (MetadataFileRef != null && FileReference.Exists(MetadataFileRef))
				{
					return MetadataFileRef.FullName;
				}
			}

			return string.Empty;
		}


		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			// don't do any target platform stuff if SDK is not available
			if (!UEBuildPlatform.IsPlatformAvailableForTarget(Platform, Target))
			{
				return;
			}
			
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				if (!Target.bBuildRequiresCookedData)
				{
					if (ModuleName == "Engine")
					{
						if (Target.bBuildDeveloperTools)
						{
							Rules.DynamicallyLoadedModuleNames.Add("HoloLensTargetPlatform");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (ModuleName == "TargetPlatform")
				{
					if (Target.bForceBuildTargetPlatforms)
					{
						Rules.DynamicallyLoadedModuleNames.Add("HoloLensTargetPlatform");
					}
				}

				if (ModuleName == "UnrealEd")
				{
					Rules.DynamicallyLoadedModuleNames.Add("HoloLensPlatformEditor");
				}
			}
			
// This code has been removed because it causes a full rebuild after generating project files (since response files are overwritten with different defines).
#if false
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				if (ProjectFileGenerator.bGenerateProjectFiles)
				{
					// Use latest SDK for Intellisense purposes
					WindowsCompiler CompilerForSdkRestriction = Target.HoloLensPlatform.Compiler != WindowsCompiler.Default ? Target.HoloLensPlatform.Compiler : Target.WindowsPlatform.Compiler;
					if (CompilerForSdkRestriction != WindowsCompiler.Default)
					{
						Version OutWin10SDKVersion;
						DirectoryReference OutSdkDir;
						if(WindowsExports.TryGetWindowsSdkDir(Target.HoloLensPlatform.Win10SDKVersionString, out OutWin10SDKVersion, out OutSdkDir))
						{
							Rules.PublicDefinitions.Add(string.Format("WIN10_SDK_VERSION={0}", OutWin10SDKVersion.Build));
						}
					}
				}
			}
#endif
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Receipt">Information about the target being deployed</param>
		public override void Deploy(TargetReceipt Receipt)
		{
			new HoloLensDeploy(Logger).PrepTargetForDeployment(Receipt);
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public override void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			if (ModuleName == "Core")
			{
				//Rules.PrivateDependencyModuleNames.Add("HoloLensSDK");
			}
			else if (ModuleName == "Engine")
			{
				Rules.PrivateDependencyModuleNames.Add("zlib");
				Rules.PrivateDependencyModuleNames.Add("UElibPNG");
				Rules.PublicDependencyModuleNames.Add("UEOgg");
				Rules.PublicDependencyModuleNames.Add("Vorbis");
			}
			else if (ModuleName == "D3D11RHI")
			{
				Rules.PublicDefinitions.Add("D3D11_WITH_DWMAPI=0");
				Rules.PublicDefinitions.Add("WITH_DX_PERF=0");
				Rules.PrivateDependencyModuleNames.Add("WinPixEventRuntime");
			}
			else if (ModuleName == "D3D12RHI")
			{
				Rules.PrivateDependencyModuleNames.Add("WinPixEventRuntime");
			}
			else if (ModuleName == "DX11")
			{
				// Clear out all the Windows include paths and libraries...
				// The HoloLensSDK module handles proper paths and libs for HoloLens.
				// However, the D3D11RHI module will include the DX11 module.
				Rules.PublicIncludePaths.Clear();
				Rules.InternalncludePaths.Clear();
				Rules.PublicSystemLibraryPaths.Clear();
				Rules.PublicSystemLibraries.Clear();
				Rules.PublicAdditionalLibraries.Clear();
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
				Rules.PrivateDependencyModuleNames.Add("WinPixEventRuntime");
			}
			else if (ModuleName == "DX11Audio")
			{
				Rules.PublicAdditionalLibraries.Remove("XAPOFX.lib");
			}
		}

		internal static void ExpandWinMDReferences(ReadOnlyTargetRules Target, string SDKFolder, string SDKVersion, ref List<string> WinMDReferences, ILogger Logger)
		{
			// Code below will fail when not using the Win10 SDK.  Early out to avoid warning spam.
			if (!Target.WindowsPlatform.bUseWindowsSDK10)
			{
				return;
			}

			if (WinMDReferences.Count > 0)
			{
				// Allow bringing in Windows SDK contracts just by naming the contract
				// These are files that look like References/10.0.98765.0/AMadeUpWindowsApiContract/5.0.0.0/AMadeUpWindowsApiContract.winmd
				List<string> ExpandedWinMDReferences = new List<string>();

				// The first few releases of the Windows 10 SDK didn't put the SDK version in the reference path
				string ReferenceRoot = Path.Combine(SDKFolder, "References");
				string VersionedReferenceRoot = Path.Combine(ReferenceRoot, SDKVersion);
				if (Directory.Exists(VersionedReferenceRoot))
				{
					ReferenceRoot = VersionedReferenceRoot;
				}

				foreach (string WinMDRef in WinMDReferences)
				{
					if (File.Exists(WinMDRef))
					{
						// Already a valid path
						ExpandedWinMDReferences.Add(WinMDRef);
					}
					else
					{
						string ContractFolder = Path.Combine(ReferenceRoot, WinMDRef);

						Version ContractVersion = FindLatestVersionDirectory(ContractFolder, null);
						string ExpandedWinMDRef = Path.Combine(ContractFolder, ContractVersion.ToString(), WinMDRef + ".winmd");
						if (File.Exists(ExpandedWinMDRef))
						{
							ExpandedWinMDReferences.Add(ExpandedWinMDRef);
						}
						else
						{
							Logger.LogWarning("Unable to resolve location for HoloLens WinMD api contract {Contract}, file {File}", WinMDRef, ExpandedWinMDRef);
						}
					}
				}

				WinMDReferences = ExpandedWinMDReferences;
			}
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			// Add Win10 SDK pieces - moved here since it allows better control over SDK version
			string? Win10SDKRoot = Target.WindowsPlatform.WindowsSdkDir;

			// Reference (WinMD) paths
			// Only Foundation and Universal are referenced by default.  
			List<string> AlwaysReferenceContracts = new List<string>();
			AlwaysReferenceContracts.Add("Windows.Foundation.FoundationContract");
			AlwaysReferenceContracts.Add("Windows.Foundation.UniversalApiContract");
			ExpandWinMDReferences(Target, Win10SDKRoot!, Target.HoloLensPlatform.Win10SDKVersion!.ToString(), ref AlwaysReferenceContracts, Logger);

			StringBuilder WinMDReferenceArguments = new StringBuilder();
			foreach (string WinMDReference in AlwaysReferenceContracts)
			{
				WinMDReferenceArguments.AppendFormat(@" /FU""{0}""", WinMDReference);
			}
			CompileEnvironment.AdditionalArguments += WinMDReferenceArguments.ToString().Trim();

			CompileEnvironment.Definitions.Add("EXCEPTIONS_DISABLED=0");

			CompileEnvironment.Definitions.Add("_WIN32_WINNT=0x0A00");
			CompileEnvironment.Definitions.Add("WINVER=0x0A00");

			CompileEnvironment.Definitions.Add("PLATFORM_HOLOLENS=1");
			CompileEnvironment.Definitions.Add("HOLOLENS=1");

			CompileEnvironment.Definitions.Add("WINAPI_FAMILY=WINAPI_FAMILY_APP");
			CompileEnvironment.Definitions.Add("PLATFORM_IS_EXTENSION=1");
			CompileEnvironment.Definitions.Add("PLATFORM_MICROSOFT=1");

			// No D3DX on HoloLens!
			CompileEnvironment.Definitions.Add("NO_D3DX_LIBS=1");

			if (Target.HoloLensPlatform.bBuildForRetailWindowsStore)
			{
				CompileEnvironment.Definitions.Add("USING_RETAIL_WINDOWS_STORE=1");
			}
			else
			{
				CompileEnvironment.Definitions.Add("USING_RETAIL_WINDOWS_STORE=0");
			}

			CompileEnvironment.Definitions.Add("WITH_D3D12_RHI=0");

			LinkEnvironment.AdditionalArguments += "/NODEFAULTLIB";
			//CompileEnvironment.AdditionalArguments += " /showIncludes";

			LinkEnvironment.SystemLibraries.Add("windowsapp.lib");

			CompileEnvironment.Definitions.Add(string.Format("WIN10_SDK_VERSION={0}", Target.HoloLensPlatform.Win10SDKVersion.Build));

			LinkEnvironment.SystemLibraries.Add("dloadhelper.lib");
			LinkEnvironment.SystemLibraries.Add("ws2_32.lib");

			if (CompileEnvironment.bUseDebugCRT)
			{
				LinkEnvironment.SystemLibraries.Add("vccorlibd.lib");
				LinkEnvironment.SystemLibraries.Add("ucrtd.lib");
				LinkEnvironment.SystemLibraries.Add("vcruntimed.lib");
				LinkEnvironment.SystemLibraries.Add("msvcrtd.lib");
				LinkEnvironment.SystemLibraries.Add("msvcprtd.lib");
			}
			else
			{
				LinkEnvironment.SystemLibraries.Add("vccorlib.lib");
				LinkEnvironment.SystemLibraries.Add("ucrt.lib");
				LinkEnvironment.SystemLibraries.Add("vcruntime.lib");
				LinkEnvironment.SystemLibraries.Add("msvcrt.lib");
				LinkEnvironment.SystemLibraries.Add("msvcprt.lib");
			}
			LinkEnvironment.SystemLibraries.Add("legacy_stdio_wide_specifiers.lib");
			LinkEnvironment.SystemLibraries.Add("uuid.lib");

			// Static CRT not supported for HoloLens
			CompileEnvironment.bUseStaticCRT = false;
			CompileEnvironment.bEnableExceptions = true;
		}

		/// <summary>
		/// Setup the configuration environment for building
		/// </summary>
		/// <param name="Target"> The target being built</param>
		/// <param name="GlobalCompileEnvironment">The global compile environment</param>
		/// <param name="GlobalLinkEnvironment">The global link environment</param>
		public override void SetUpConfigurationEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			// Determine the C++ compile/link configuration based on the Unreal configuration.

			if (GlobalCompileEnvironment.bUseDebugCRT)
			{
				GlobalCompileEnvironment.Definitions.Add("_DEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}

			//CppConfiguration CompileConfiguration;
			UnrealTargetConfiguration CheckConfig = Target.Configuration;
			switch (CheckConfig)
			{
				default:
				case UnrealTargetConfiguration.Debug:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_DEBUG=1");
					break;
				case UnrealTargetConfiguration.DebugGame:
				// Default to Development; can be overriden by individual modules.
				case UnrealTargetConfiguration.Development:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_DEVELOPMENT=1");
					break;
				case UnrealTargetConfiguration.Shipping:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_SHIPPING=1");
					break;
				case UnrealTargetConfiguration.Test:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_TEST=1");
					break;
			}

			// Create debug info based on the heuristics specified by the user.
			GlobalCompileEnvironment.bCreateDebugInfo =
				!Target.bDisableDebugInfo && ShouldCreateDebugInfo(Target);

			// NOTE: Even when debug info is turned off, we currently force the linker to generate debug info
			//	   anyway on Visual C++ platforms.  This will cause a PDB file to be generated with symbols
			//	   for most of the classes and function/method names, so that crashes still yield somewhat
			//	   useful call stacks, even though compiler-generate debug info may be disabled.  This gives
			//	   us much of the build-time savings of fully-disabled debug info, without giving up call
			//	   data completely.
			GlobalLinkEnvironment.bCreateDebugInfo = true;
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool	true if debug info should be generated, false if not</returns>
		public override bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target)
		{
			switch (Target.Configuration)
			{
				case UnrealTargetConfiguration.Development:
				case UnrealTargetConfiguration.Shipping:
				case UnrealTargetConfiguration.Test:
					return !Target.bOmitPCDebugInfoInDevelopment;
				case UnrealTargetConfiguration.DebugGame:
				case UnrealTargetConfiguration.Debug:
				default:
					return true;
			};
		}

		/// <summary>
		/// Creates a toolchain instance for the given platform.
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public override UEToolChain CreateToolChain(ReadOnlyTargetRules Target)
		{
			return new HoloLensToolChain(Target, Logger);
		}

		/// <summary>
		/// Check for the build requirement due to platform requirements
		/// return true if the project requires a build
		/// </summary>
		public override bool RequiresBuild(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			// Hololens requires UBT run before packaging so that HoloLensDeploy PrepTargetForDeployment will create the Multi.target file which is used in hololens packaging.
			return true;
		}
	}



	class HoloLensPlatformFactory : UEBuildPlatformFactory
	{
		public override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.HoloLens; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms(ILogger Logger)
		{
			if (OperatingSystem.IsWindows())
			{
				// for GetValidSoftwareVersionRange reasons we probably want a HoloLensePlatformSDK class
				MicrosoftPlatformSDK SDK = new MicrosoftPlatformSDK(Logger);

				UEBuildPlatform.RegisterBuildPlatform(new HoloLensPlatform(SDK, Logger), Logger);
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.HoloLens, UnrealPlatformGroup.Microsoft);
#pragma warning disable CS0618 // Type or member is obsolete
				UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.HoloLens, UnrealPlatformGroup.HoloLens);
#pragma warning restore CS0618
				}
			}
		}
}
