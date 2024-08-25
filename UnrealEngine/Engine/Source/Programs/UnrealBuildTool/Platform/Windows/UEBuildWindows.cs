// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Runtime.Versioning;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	partial struct UnrealArch
	{
		/// <summary>
		/// Version of Arm64 that can interop with X64 (Emulation Compatible)
		/// </summary>
		public static UnrealArch Arm64ec = FindOrAddByName("arm64ec", bIsX64: false);

		private struct WindowsArchInfo
		{
			public string ToolChain;
			public string LibDir;
			public string SystemLibDir;

			public WindowsArchInfo(string ToolChain, string LibDir, string SystemLibDir)
			{
				this.ToolChain = ToolChain;
				this.LibDir = LibDir;
				this.SystemLibDir = SystemLibDir;
			}
		}

		private static IReadOnlyDictionary<UnrealArch, WindowsArchInfo> WindowsToolchainArchitectures = new Dictionary<UnrealArch, WindowsArchInfo>()
		{
			{ UnrealArch.Arm64,         new WindowsArchInfo("arm64", "arm64", "arm64") },
			{ UnrealArch.Arm64ec,       new WindowsArchInfo("arm64", "x64", "arm64") },
			{ UnrealArch.X64,           new WindowsArchInfo("x64", "x64", "x64") },
		};

		/// <summary>
		/// Windows-specific tool chain to compile with
		/// </summary>
		public string WindowsToolChain
		{
			get
			{
				if (WindowsToolchainArchitectures.ContainsKey(this))
				{
					return WindowsToolchainArchitectures[this].ToolChain;
				}

				throw new BuildException($"Unknown architecture {ToString()} passed to UnrealArch.WindowsToolChain");
			}
		}

		/// <summary>
		/// Windows-specific lib directory for this architecture
		/// </summary>
		public string WindowsLibDir
		{
			get
			{
				if (WindowsToolchainArchitectures.ContainsKey(this))
				{
					return WindowsToolchainArchitectures[this].LibDir;
				}

				throw new BuildException($"Unknown architecture {ToString()} passed to UnrealArch.WindowsLibDir");
			}
		}

		/// <summary>
		/// Windows-specific system lib directory for this architecture
		/// </summary>
		public string WindowsSystemLibDir
		{
			get
			{
				if (WindowsToolchainArchitectures.ContainsKey(this))
				{
					return WindowsToolchainArchitectures[this].SystemLibDir;
				}

				throw new BuildException($"Unknown architecture {ToString()} passed to UnrealArch.WindowsSystemLibDir");
			}
		}

		/// <summary>
		/// Windows-specific low level name for the generic platforms
		/// </summary>
		public string WindowsName
		{
			get
			{
				if (WindowsToolchainArchitectures.ContainsKey(this))
				{
					return WindowsToolchainArchitectures[this].ToolChain;
				}

				throw new BuildException($"Unknown architecture {ToString()} passed to UnrealArch.WindowsName");
			}
		}
	}

	/// <summary>
	/// Available compiler toolchains on Windows platform
	/// </summary>
	public enum WindowsCompiler
	{
		/// <summary>
		/// Use the default compiler. A specific value will always be used outside of configuration classes.
		/// </summary>
		Default,

		/// <summary>
		/// Use Clang for Windows, using the clang-cl driver.
		/// </summary>
		Clang,

		/// <summary>
		/// Use the RTFM (Rollback Transactions on Failure Memory) Clang variant for Verse on Windows, using the verse-clang-cl driver.
		/// </summary>
		ClangRTFM,

		/// <summary>
		/// Use the Intel oneAPI C++ compiler
		/// </summary>
		Intel,

		/// <summary>
		/// Visual Studio 2022 (Visual C++ 17.0)
		/// </summary>
		VisualStudio2022,

		/// <summary>
		/// Unsupported Visual Studio
		/// Must be the last entry in WindowsCompiler enum and should only be used in limited circumstances
		/// </summary>
		[Obsolete("Unsupported Visual Studio WindowsCompiler, do not use this enum")]
		VisualStudioUnsupported,
	}

	/// <summary>
	/// Enum describing the release channel of a compiler
	/// </summary>
	internal enum WindowsCompilerChannel
	{
		Latest,
		Preview,
		Experimental,
		Any = Latest | Preview | Experimental
	}

	/// <summary>
	/// Extension methods for WindowsCompiler enum
	/// </summary>
	public static class WindowsCompilerExtensions
	{
		/// <summary>
		/// Returns if this compiler toolchain based on Clang
		/// </summary>
		/// <param name="Compiler">The compiler to check</param>
		/// <returns>true if Clang based</returns>
		public static bool IsClang(this WindowsCompiler Compiler)
		{
			return Compiler == WindowsCompiler.Clang || Compiler == WindowsCompiler.ClangRTFM || Compiler == WindowsCompiler.Intel;
		}

		/// <summary>
		/// Returns if this compiler toolchain based on Intel
		/// </summary>
		/// <param name="Compiler">The compiler to check</param>
		/// <returns>true if Intel based</returns>
		public static bool IsIntel(this WindowsCompiler Compiler)
		{
			return Compiler == WindowsCompiler.Intel;
		}

		/// <summary>
		/// Returns if this compiler toolchain based on MSVC
		/// </summary>
		/// <param name="Compiler">The compiler to check</param>
		/// <returns>true if MSVC based</returns>
		public static bool IsMSVC(this WindowsCompiler Compiler)
		{
			return Compiler >= WindowsCompiler.VisualStudio2022;
		}
	}

	/// <summary>
	/// Windows-specific target settings
	/// </summary>
	public class WindowsTargetRules
	{
		/// <summary>
		/// The target rules which owns this object. Used to resolve some properties.
		/// </summary>
		TargetRules Target;

		/// <summary>
		/// If enabled will set the ProductVersion embeded in windows executables and dlls to contain BUILT_FROM_CHANGELIST and BuildVersion
		/// Enabled by default for all precompiled and Shipping configurations. Regardless of this setting, the versions from Build.version will be available via the BuildSettings module
		/// Note: Embedding these versions will cause resource files to be recompiled whenever changelist is updated which will cause binaries to relink
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "SetResourceVersions")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-SetResourceVersions")]
		[CommandLine("-NoSetResourceVersions", Value = "false")]
		public bool bSetResourceVersions
		{
			get => bSetResourceVersionPrivate ?? Target.bPrecompile || Target.Configuration == UnrealTargetConfiguration.Shipping;
			set => bSetResourceVersionPrivate = value;
		}
		private bool? bSetResourceVersionPrivate = null;

		/// <summary>
		/// If -PGOOptimize is specified but the linker flags have changed since the last -PGOProfile, this will emit a warning and build without PGO instead of failing during link with LNK1268. 
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-IgnoreStalePGOData")]
		public bool bIgnoreStalePGOData = false;

		/// <summary>
		/// If specified along with -PGOProfile, then /FASTGENPROFILE will be used instead of /GENPROFILE. This usually means that the PGO data is generated faster, but the resulting data may not yield as efficient optimizations during -PGOOptimize
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-PGOFastGen")]
		public bool bUseFastGenProfile = false;

		/// <summary>
		/// If specified along with -PGOProfile, prevent the usage of extra counters. Please note that by default /FASTGENPROFILE doesnt use extra counters
		/// </summary>
		/// <seealso href="https://learn.microsoft.com/en-us/cpp/build/reference/genprofile-fastgenprofile-generate-profiling-instrumented-build">genprofile-fastgenprofile-generate-profiling-instrumented-build</seealso>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-PGONoExtraCounters")]
		public bool bPGONoExtraCounters = false;

		/// <summary>
		/// If specified along with -PGOProfile, use sample-based PGO instead of instrumented. Currently Intel oneAPI 2024.0+ only.
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-SampleBasedPGO")]
		public bool bSampleBasedPGO = false;

		/// <summary>
		/// Which level to use for Inline Function Expansion when TargetRules.bUseInlining is enabled
		/// </summary>
		/// <seealso href="https://learn.microsoft.com/en-us/cpp/build/reference/ob-inline-function-expansion">ob-inline-function-expansion</seealso>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "InlineFunctionExpansionLevel")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		public int InlineFunctionExpansionLevel { get; set; } = 2;

		/// <summary>
		/// Version of the compiler toolchain to use on Windows platform. A value of "default" will be changed to a specific version at UBT start up.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "Compiler")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-2022", Value = nameof(WindowsCompiler.VisualStudio2022))]
		[CommandLine("-Compiler=")]
		public WindowsCompiler Compiler = WindowsCompiler.Default;

		/// <summary>
		/// Version of the toolchain to use on Windows platform when a non-msvc Compiler is in use, to locate include paths etc.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "Toolchain")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-VCToolchain=")]
		public WindowsCompiler ToolChain
		{
			get => ToolChainPrivate ?? (Compiler.IsMSVC() ? Compiler : WindowsPlatform.GetDefaultCompiler(Target.ProjectFile, Architecture, Target.Logger, true));
			set => ToolChainPrivate = value;
		}
		private WindowsCompiler? ToolChainPrivate = null;

		/// <summary>
		/// Architecture of Target.
		/// </summary>
		public UnrealArch Architecture
		{
			get;
			internal set;
		}
		= UnrealArch.X64;

		/// <summary>
		/// Warning level when reporting toolchains that are not in the preferred version list
		/// </summary>
		/// <seealso cref="MicrosoftPlatformSDK.PreferredVisualCppVersions"/>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "ToolchainVersionWarningLevel")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-ToolchainVersionWarningLevel=")]
		public WarningLevel ToolchainVersionWarningLevel { get; set; } = WarningLevel.Warning;

		/// <summary>
		/// The specific compiler version to use. This may be a specific version number (for example, "14.13.26128"), the string "Latest" to select the newest available version, or
		/// the string "Preview" to select the newest available preview version. By default, and if it is available, we use the toolchain version indicated by
		/// WindowsPlatform.DefaultToolChainVersion (otherwise, we use the latest version).
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "CompilerVersion")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-CompilerVersion")]
		public string? CompilerVersion = null;

		/// <summary>
		/// The specific msvc toolchain version to use if the compiler is not msvc. This may be a specific version number (for example, "14.13.26128"), the string "Latest" to select the newest available version, or
		/// the string "Preview" to select the newest available preview version. By default, and if it is available, we use the toolchain version indicated by
		/// WindowsPlatform.DefaultToolChainVersion (otherwise, we use the latest version).
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "ToolchainVersion")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-VCToolchainVersion")]
		public string? ToolchainVersion = null;

		/// <summary>
		/// True if /fastfail should be passed to the msvc compiler and linker
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "bVCFastFail")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-VCFastFail")]
		public bool bVCFastFail = false;

		/// <summary>
		/// True if /d2ExtendedWarningInfo should be passed to the compiler and /d2:-ExtendedWarningInfo to the linker
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "bVCExtendedWarningInfo")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-VCExtendedWarningInfo")]
		[CommandLine("-VCDisableExtendedWarningInfo", Value ="false")]
		public bool bVCExtendedWarningInfo = true;

		/// <summary>
		/// True if optimizations to reduce the size of debug information should be disabled
		/// See https://clang.llvm.org/docs/UsersManual.html#cmdoption-fstandalone-debug for more information
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "bClangStandaloneDebug")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-ClangStandaloneDebug")]
		public bool bClangStandaloneDebug = false;

		/// <summary>
		/// True if we should use the Clang linker (LLD) when we are compiling with Clang or Intel oneAPI, otherwise we use the MSVC linker.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "bAllowClangLinker")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-ClangLinker")]
		public bool bAllowClangLinker = false;

		/// <summary>
		/// The specific Windows SDK version to use. This may be a specific version number (for example, "8.1", "10.0" or "10.0.10150.0"), or the string "Latest", to select the newest available version.
		/// By default, and if it is available, we use the Windows SDK version indicated by WindowsPlatform.DefaultWindowsSdkVersion (otherwise, we use the latest version).
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "WindowsSDKVersion")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-WindowsSDKVersion")]
		public string? WindowsSdkVersion = null;

		/// <summary>
		/// Value for the WINVER macro, defining the minimum supported Windows version.
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "TargetWindowsVersion")]
		public int TargetWindowsVersion = 0x601;

		/// <summary>
		/// Value for the NTDDI_VERSION macro, defining the minimum supported Windows version.
		/// https://learn.microsoft.com/en-us/windows/win32/winprog/using-the-windows-headers?redirectedfrom=MSDN#macros-for-conditional-declarations
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "TargetWindowsMinorVersion")]
		public int? TargetWindowsMinorVersion = null;

		/// <summary>
		/// Enable PIX debugging (automatically disabled in Shipping and Test configs)
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "bEnablePIXProfiling")]
		public bool bPixProfilingEnabled = true;

		/// <summary>
		/// Enable building with the Win10 SDK instead of the older Win8.1 SDK 
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "bUseWindowsSDK10")]
		public bool bUseWindowsSDK10 = false;

		/// <summary>
		/// Enable building with the C++/WinRT language projection
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "bUseCPPWinRT")]
		public bool bUseCPPWinRT = false;

		/// <summary>
		/// Enables runtime ray tracing support.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "bEnableRayTracing")]
		public bool bEnableRayTracing = false;

		/// <summary>
		/// The name of the company (author, provider) that created the project.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Game, "/Script/EngineSettings.GeneralProjectSettings", "CompanyName")]
		public string? CompanyName;

		/// <summary>
		/// The project's copyright and/or trademark notices.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Game, "/Script/EngineSettings.GeneralProjectSettings", "CopyrightNotice")]
		public string? CopyrightNotice;

		/// <summary>
		/// The product name.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Game, "/Script/EngineSettings.GeneralProjectSettings", "ProjectName")]
		public string? ProductName;

		/// <summary>
		/// Enables address sanitizer (ASan). Only supported for Visual Studio 2019 16.7.0 and up.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableAddressSanitizer")]
		[CommandLine("-EnableASan")]
		public bool bEnableAddressSanitizer = false;

		/// <summary>
		/// Enables LibFuzzer. Only supported for Visual Studio 2022 17.0.0 and up.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableLibFuzzer")]
		[CommandLine("-EnableLibFuzzer")]
		public bool bEnableLibFuzzer = false;

		/// <summary>
		/// Whether .sarif files containing errors and warnings are written alongside each .obj, if supported
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bWriteSarif")]
		[CommandLine("-Sarif")]
		public bool bWriteSarif = false;

		/// <summary>
		/// Whether we should export a file containing .obj to source file mappings.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-ObjSrcMap")]
		public string? ObjSrcMapFile = null;

		/// <summary>
		/// Whether to have the linker or library tool to generate a link repro in the specified directory
		/// See https://learn.microsoft.com/en-us/cpp/build/reference/linkrepro for more information
		/// </summary>
		[CommandLine("-LinkRepro=")]
		public string? LinkReproDir = null;

		/// <summary>
		/// Provides a Module Definition File (.def) to the linker to describe various attributes of a DLL.
		/// Necessary when exporting functions by ordinal values instead of by name.
		/// </summary>
		public string? ModuleDefinitionFile;

		/// <summary>
		/// Specifies the path to a manifest file for the linker to embed. Defaults to the manifest in Engine/Build/Windows/Resources. Can be assigned to null
		/// if the target wants to specify its own manifest.
		/// </summary>
		public string? ManifestFile;

		/// <summary>
		/// Specifies the path to an .ico file to use as the appliction icon. Can be assigned to null and will default to Engine/Build/Windows/Resources/Default.ico for engine targets or Build/Windows/Application.ico for projects.
		/// </summary>
		public string? ApplicationIcon;

		/// <summary>
		/// Enables strict standard conformance mode (/permissive-).
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-Strict")]
		public bool bStrictConformanceMode
		{
			get => bStrictConformanceModePrivate ?? Target.DefaultBuildSettings >= BuildSettingsVersion.V4;
			set => bStrictConformanceModePrivate = value;
		}
		private bool? bStrictConformanceModePrivate;

		/// <summary>
		/// Enables updated __cplusplus macro (/Zc:__cplusplus).
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-UpdatedCPPMacro")]
		public bool bUpdatedCPPMacro = true;

		/// <summary>
		/// Enables inline conformance (Remove unreferenced COMDAT) (/Zc:inline).
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-StrictInline")]
		public bool bStrictInlineConformance = false;

		/// <summary>
		/// Enables new preprocessor conformance (/Zc:preprocessor). This is always enabled for C++20 modules.
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-StrictPreprocessor")]
		public bool bStrictPreprocessorConformance = false;

		/// <summary>
		/// Enables enum types conformance (/Zc:enumTypes) in VS2022 17.4 Preview 4.0+.
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-StrictEnumTypes")]
		public bool bStrictEnumTypesConformance = false;

		/// <summary>
		/// Whether to request the linker create a stripped pdb file as part of the build.
		/// If enabled the full debug pdb will have the extension .full.pdb
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-StripPrivateSymbols")]
		public bool bStripPrivateSymbols = false;

		/// <summary>
		/// Set page size to allow for larger than 4GB PDBs to be generated by the msvc linker.
		/// Will default to 16384 for monolithic editor builds.
		/// Values should be a power of two such as 4096, 8192, 16384, or 32768
		/// </summary>
		public uint? PdbPageSize = null;

		/// <summary>
		/// Specify an alternate location for the PDB file. This option does not change the location of the generated PDB file,
		/// it changes the name that is embedded into the executable. Path can contain %_PDB% which will be expanded to the original
		/// PDB file name of the target, without the directory.
		/// See https://learn.microsoft.com/en-us/cpp/build/reference/pdbaltpath-use-alternate-pdb-path
		/// </summary>
		[CommandLine("-PdbAltPath")]
		public string? PdbAlternatePath = null;

		/// VS2015 updated some of the CRT definitions but not all of the Windows SDK has been updated to match.
		/// Microsoft provides legacy_stdio_definitions library to enable building with VS2015 until they fix everything up.
		public bool bNeedsLegacyStdioDefinitionsLib => Compiler.IsMSVC() || Compiler.IsClang();

		/// <summary>
		/// The stack size when linking
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings")]
		public int DefaultStackSize = 12000000;

		/// <summary>
		/// The stack size to commit when linking
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings")]
		public int DefaultStackSizeCommit;

		/// <summary>
		/// Max number of slots FWindowsPlatformTLS::AllocTlsSlot can allocate.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsRuntimeSettings.WindowsRuntimeSettings")]
		public int MaxNumTlsSlots = 0;

		/// <summary>
		/// Max number threads that can use FWindowsPlatformTLS at one time.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsRuntimeSettings.WindowsRuntimeSettings")]
		public int MaxNumThreadsWithTlsSlots = 0;

		/// <summary>
		/// Determines the amount of memory that the compiler allocates to construct precompiled headers (/Zm).
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		public int PCHMemoryAllocationFactor = 0;

		/// <summary>
		/// Allow the target to specify extra options for linking that aren't otherwise noted here
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		public string AdditionalLinkerOptions = "";

		/// <summary>
		/// Create an image that can be hot patched (/FUNCTIONPADMIN)
		/// </summary>
		public bool bCreateHotPatchableImage
		{
			get => bCreateHotPatchableImagePrivate ?? Target.bWithLiveCoding;
			set => bCreateHotPatchableImagePrivate = value;
		}
		private bool? bCreateHotPatchableImagePrivate;

		/// <summary>
		/// Strip unreferenced symbols (/OPT:REF)
		/// </summary>
		public bool bStripUnreferencedSymbols
		{
			get => bStripUnreferencedSymbolsPrivate ?? ((Target.Configuration == UnrealTargetConfiguration.Test || Target.Configuration == UnrealTargetConfiguration.Shipping) && !Target.bWithLiveCoding);
			set => bStripUnreferencedSymbolsPrivate = value;
		}
		private bool? bStripUnreferencedSymbolsPrivate;

		/// <summary>
		/// Merge identical COMDAT sections together (/OPT:ICF)
		/// </summary>
		public bool bMergeIdenticalCOMDATs
		{
			get => bMergeIdenticalCOMDATsPrivate ?? ((Target.Configuration == UnrealTargetConfiguration.Test || Target.Configuration == UnrealTargetConfiguration.Shipping) && !Target.bWithLiveCoding);
			set => bMergeIdenticalCOMDATsPrivate = value;
		}
		private bool? bMergeIdenticalCOMDATsPrivate;

		/// <summary>
		/// Whether to put global symbols in their own sections (/Gw), allowing the linker to discard any that are unused.
		/// </summary>
		public bool bOptimizeGlobalData = true;

		/// <summary>
		/// (Experimental) Appends the -ftime-trace argument to the command line for Clang to output a JSON file containing a timeline for the compile. 
		/// See http://aras-p.info/blog/2019/01/16/time-trace-timeline-flame-chart-profiler-for-Clang/ for more info.
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		public bool bClangTimeTrace = false;

		/// <summary>
		/// Outputs compile timing information so that it can be analyzed.
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		public bool bCompilerTrace = false;

		/// <summary>
		/// Print out files that are included by each source file
		/// </summary>
		[CommandLine("-ShowIncludes")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		public bool bShowIncludes = false;

		/// <summary>
		/// Bundle a working version of dbghelp.dll with the application, and use this to generate minidumps. This works around a bug with the Windows 10 Fall Creators Update (1709)
		/// where rich PE headers larger than a certain size would result in corrupt minidumps.
		/// </summary>
		public bool bUseBundledDbgHelp = true;

		/// <summary>
		/// Whether this build will use Microsoft's custom XCurl instead of libcurl
		/// Note that XCurl is not part of the normal Windows SDK and will require additional downloads
		/// </summary>
		[CommandLine("-UseXCurl")]
		public bool bUseXCurl = false;

		/// <summary>
		/// Settings for PVS studio
		/// </summary>
		public PVSTargetSettings PVS = new PVSTargetSettings();

		/// <summary>
		/// The Visual C++ environment to use for this target. Only initialized after all the target settings are finalized, in ValidateTarget().
		/// </summary>
		internal VCEnvironment? Environment;

		/// <summary>
		/// Directory containing the NETFXSDK
		/// </summary>
		[SuppressMessage("Interoperability", "CA1416:Validate platform compatibility", Justification = "Manually checked")]
		public string? NetFxSdkDir
		{
			get
			{
				DirectoryReference? NetFxSdkDir;
				if (RuntimePlatform.IsWindows && MicrosoftPlatformSDK.TryGetNetFxSdkInstallDir(out NetFxSdkDir))
				{
					return NetFxSdkDir.FullName;
				}
				return null;
			}
		}

		/// <summary>
		/// Directory containing the DIA SDK
		/// </summary>
		public string? DiaSdkDir => MicrosoftPlatformSDK.FindDiaSdkDirs(Environment!.ToolChain).Select(x => x.FullName).FirstOrDefault();

		/// <summary>
		/// Directory containing the IDE package (Professional, Community, etc...)
		/// </summary>
		public string? IDEDir
		{
			get
			{
				try
				{
					return MicrosoftPlatformSDK.FindVisualStudioInstallations(Environment!.ToolChain, Target.Logger).Select(x => x.BaseDir.FullName).FirstOrDefault();
				}
				catch (Exception) // Find function will throw if there is no visual studio installed! This can happen w/ clang builds
				{
					return null;
				}
			}
		}

		/// <summary>
		/// Directory containing ThirdParty DirectX
		/// </summary>
		public string DirectXDir => Path.Combine(Unreal.EngineSourceDirectory.FullName, "ThirdParty", "Windows", "DirectX");

		/// <summary>
		/// Directory containing ThirdParty DirectX libs
		/// </summary>
		public string DirectXLibDir => Path.Combine(DirectXDir, "Lib", Target.Architecture.WindowsLibDir) + "/";

		/// <summary>
		/// Directory containing ThirdParty DirectX dlls
		/// </summary>
		public string DirectXDllDir => Path.Combine(Unreal.EngineDirectory.FullName, "Binaries", "ThirdParty", "Windows", "DirectX", Target.Architecture.WindowsLibDir) + "/";

		/// <summary>
		/// When using a Visual Studio compiler, returns the version name as a string
		/// </summary>
		/// <returns>The Visual Studio compiler version name (e.g. "2022")</returns>
		public string GetVisualStudioCompilerVersionName()
		{
			switch (Compiler)
			{
				case WindowsCompiler.Clang:
				case WindowsCompiler.ClangRTFM:
				case WindowsCompiler.Intel:
				case WindowsCompiler.VisualStudio2022:
					return "2015"; // VS2022 is backwards compatible with VS2015 compiler

				default:
					throw new BuildException("Unexpected WindowsCompiler version for GetVisualStudioCompilerVersionName().  Either not using a Visual Studio compiler or switch block needs to be updated");
			}
		}

		/// <summary>
		/// Determines if a given compiler is installed and valid
		/// </summary>
		/// <param name="Compiler">Compiler to check for</param>
		/// <param name="Architecture">Architecture the compiler must support</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>True if the given compiler is installed and valid</returns>
		public static bool HasValidCompiler(WindowsCompiler Compiler, UnrealArch Architecture, ILogger Logger)
		{
			return MicrosoftPlatformSDK.HasValidCompiler(Compiler, Architecture, Logger);
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Target">The target rules which owns this object</param>
		internal WindowsTargetRules(TargetRules Target)
		{
			this.Target = Target;

			string Platform = Target.Platform.ToString();
			if (Target.Platform == UnrealTargetPlatform.Win64 && !Target.Architecture.bIsX64)
			{
				Platform += "-arm64";
			}
			ManifestFile = FileReference.Combine(Unreal.EngineDirectory, "Build", "Windows", "Resources", String.Format("Default-{0}.manifest", Platform)).FullName;
		}
	}

	/// <summary>
	/// Read-only wrapper for Windows-specific target settings
	/// </summary>
	public class ReadOnlyWindowsTargetRules
	{
		/// <summary>
		/// The private mutable settings object
		/// </summary>
		private WindowsTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyWindowsTargetRules(WindowsTargetRules Inner)
		{
			this.Inner = Inner;
			PVS = new ReadOnlyPVSTargetSettings(Inner.PVS);
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>
		#region Read-only accessor properties 
#pragma warning disable CS1591

		public bool bSetResourceVersions => Inner.bSetResourceVersions;

		public bool bIgnoreStalePGOData => Inner.bIgnoreStalePGOData;

		public bool bUseFastGenProfile => Inner.bUseFastGenProfile;

		public bool bPGONoExtraCounters => Inner.bPGONoExtraCounters;

		public bool bSampleBasedPGO => Inner.bSampleBasedPGO;

		public int InlineFunctionExpansionLevel => Inner.InlineFunctionExpansionLevel;

		public WindowsCompiler Compiler => Inner.Compiler;

		public WindowsCompiler ToolChain => Inner.ToolChain;

		public UnrealArch Architecture => Inner.Architecture;

		public WarningLevel ToolchainVersionWarningLevel => Inner.ToolchainVersionWarningLevel;

		public string? CompilerVersion => Inner.CompilerVersion;

		public string? ToolchainVerison => Inner.ToolchainVersion;

		public string? WindowsSdkVersion => Inner.WindowsSdkVersion;

		public int TargetWindowsVersion => Inner.TargetWindowsVersion;

		public int? TargetWindowsMinorVersion => Inner.TargetWindowsMinorVersion;

		public bool bPixProfilingEnabled => Inner.bPixProfilingEnabled;

		public bool bUseWindowsSDK10 => Inner.bUseWindowsSDK10;

		public bool bUseCPPWinRT => Inner.bUseCPPWinRT;

		public bool bVCFastFail => Inner.bVCFastFail;

		public bool bVCExtendedWarningInfo => Inner.bVCExtendedWarningInfo;

		public bool bClangStandaloneDebug => Inner.bClangStandaloneDebug;

		public bool bAllowClangLinker => Inner.bAllowClangLinker;

		public bool bEnableRayTracing => Inner.bEnableRayTracing;

		public string? CompanyName => Inner.CompanyName;

		public string? CopyrightNotice => Inner.CopyrightNotice;

		public string? ProductName => Inner.ProductName;

		public bool bEnableAddressSanitizer => Inner.bEnableAddressSanitizer;

		public bool bEnableLibFuzzer => Inner.bEnableLibFuzzer;

		public bool bWriteSarif => Inner.bWriteSarif;

		public string? ObjSrcMapFile => Inner.ObjSrcMapFile;

		public string? LinkReproDir => Inner.LinkReproDir;

		public string? ModuleDefinitionFile => Inner.ModuleDefinitionFile;

		public string? ManifestFile => Inner.ManifestFile;

		public string? ApplicationIcon => Inner.ApplicationIcon;

		public bool bNeedsLegacyStdioDefinitionsLib => Inner.bNeedsLegacyStdioDefinitionsLib;

		public bool bStrictConformanceMode => Inner.bStrictConformanceMode;

		public bool bUpdatedCPPMacro => Inner.bUpdatedCPPMacro;

		public bool bStrictInlineConformance => Inner.bStrictInlineConformance;

		public bool bStrictPreprocessorConformance => Inner.bStrictPreprocessorConformance;

		public bool bStrictEnumTypesConformance => Inner.bStrictEnumTypesConformance;

		public bool bStripPrivateSymbols => Inner.bStripPrivateSymbols;

		public uint? PdbPageSize => Inner.PdbPageSize;

		public string? PdbAlternatePath => Inner.PdbAlternatePath;

		public int DefaultStackSize => Inner.DefaultStackSize;

		public int DefaultStackSizeCommit => Inner.DefaultStackSizeCommit;

		public int PCHMemoryAllocationFactor => Inner.PCHMemoryAllocationFactor;

		public string AdditionalLinkerOptions => Inner.AdditionalLinkerOptions;

		public bool bCreateHotpatchableImage => Inner.bCreateHotPatchableImage;

		public bool bStripUnreferencedSymbols => Inner.bStripUnreferencedSymbols;

		public bool bMergeIdenticalCOMDATs => Inner.bMergeIdenticalCOMDATs;

		public bool bOptimizeGlobalData => Inner.bOptimizeGlobalData;

		public bool bClangTimeTrace => Inner.bClangTimeTrace;

		public bool bCompilerTrace => Inner.bCompilerTrace;

		public bool bShowIncludes => Inner.bShowIncludes;

		public string GetVisualStudioCompilerVersionName()
		{
			return Inner.GetVisualStudioCompilerVersionName();
		}

		public bool bUseBundledDbgHelp => Inner.bUseBundledDbgHelp;

		public bool bUseXCurl => Inner.bUseXCurl;

		public ReadOnlyPVSTargetSettings PVS
		{
			get; private set;
		}

		internal VCEnvironment? Environment => Inner.Environment;


		public string? ToolChainDir => Inner.Environment?.ToolChainDir.FullName ?? null;

		public string? ToolChainVersion => Inner.Environment?.ToolChainVersion.ToString() ?? null;

		public string? WindowsSdkDir => Inner.Environment?.WindowsSdkDir.ToString() ?? null;

		public string? NetFxSdkDir => Inner.NetFxSdkDir;

		public string? DiaSdkDir => Inner.DiaSdkDir;

		public string? IDEDir => Inner.IDEDir;

		public string DirectXDir => Inner.DirectXDir;

		public string DirectXLibDir => Inner.DirectXLibDir;

		public string DirectXDllDir => Inner.DirectXDllDir;

		public int MaxNumTlsSlots => Inner.MaxNumTlsSlots;

		public int MaxNumThreadsWithTlsSlots => Inner.MaxNumThreadsWithTlsSlots;


#pragma warning restore CS1591
		#endregion
	}

	/// <summary>
	/// Information about a particular Visual Studio installation
	/// </summary>
	[DebuggerDisplay("{BaseDir}")]
	class VisualStudioInstallation
	{
		/// <summary>
		/// Compiler type
		/// </summary>
		public WindowsCompiler Compiler { get; }

		/// <summary>
		/// Version number for this installation
		/// </summary>
		public VersionNumber Version { get; }

		/// <summary>
		/// Base directory for the installation
		/// </summary>
		public DirectoryReference BaseDir { get; }

		/// <summary>
		/// Whether it's a community edition of Visual Studio.
		/// </summary>
		public bool bCommunity { get; }

		/// <summary>
		/// The release channel of this installation
		/// </summary>
		public WindowsCompilerChannel ReleaseChannel { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public VisualStudioInstallation(WindowsCompiler Compiler, VersionNumber Version, DirectoryReference BaseDir, bool bCommunity, WindowsCompilerChannel ReleaseChannel)
		{
			this.Compiler = Compiler;
			this.Version = Version;
			this.BaseDir = BaseDir;
			this.bCommunity = bCommunity;
			this.ReleaseChannel = ReleaseChannel;
		}
	}

	class WindowsArchitectureConfig : UnrealArchitectureConfig
	{
		public WindowsArchitectureConfig()
			: base(UnrealArchitectureMode.OneTargetPerArchitecture, new[] { UnrealArch.X64, UnrealArch.Arm64, UnrealArch.Arm64ec })
		{

		}

		public override UnrealArchitectures ActiveArchitectures(FileReference? ProjectFile, string? TargetName)
		{
			// for now always compile X64 unless overridden on commandline
			return new UnrealArchitectures(UnrealArch.X64);
		}

		public override bool RequiresArchitectureFilenames(UnrealArchitectures Architectures)
		{
			return Architectures.SingleArchitecture != UnrealArch.X64;
		}

		public override UnrealArch GetHostArchitecture()
		{
			switch (System.Runtime.InteropServices.RuntimeInformation.ProcessArchitecture)
			{
				case System.Runtime.InteropServices.Architecture.Arm64:
					return UnrealArch.Arm64;
				default: 
					return UnrealArch.X64;
			}
		}
	}

	class WindowsPlatform : UEBuildPlatform
	{
		MicrosoftPlatformSDK SDK;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InPlatform">Creates a windows platform with the given enum value</param>
		/// <param name="InSDK">The installed Windows SDK</param>
		/// <param name="InLogger">Logger instance</param>
		public WindowsPlatform(UnrealTargetPlatform InPlatform, MicrosoftPlatformSDK InSDK, ILogger InLogger)
			: this(InPlatform, InSDK, new WindowsArchitectureConfig(), InLogger)
		{
		}

		/// <summary>
		/// Constructor that takes an archConfig (for use by subclasses)
		/// </summary>
		/// <param name="InPlatform">Creates a windows platform with the given enum value</param>
		/// <param name="InSDK">The installed Windows SDK</param>
		/// <param name="ArchConfig">Achitecture configuration</param>
		/// <param name="InLogger">Logger instance</param>
		public WindowsPlatform(UnrealTargetPlatform InPlatform, MicrosoftPlatformSDK InSDK, UnrealArchitectureConfig ArchConfig, ILogger InLogger)
			: base(InPlatform, InSDK, ArchConfig, InLogger)
		{
			SDK = InSDK;
		}
		/// <summary>
		/// Reset a target's settings to the default
		/// </summary>
		/// <param name="Target"></param>
		public override void ResetTarget(TargetRules Target)
		{
			base.ResetTarget(Target);
		}

		/// <summary>
		/// Creates the VCEnvironment object used to control compiling and other tools. Virtual to allow other platforms to override behavior
		/// </summary>
		/// <param name="Target">Stanard target object</param>
		/// <returns></returns>
		[SupportedOSPlatform("windows")]
		protected virtual VCEnvironment CreateVCEnvironment(TargetRules Target)
		{
			return VCEnvironment.Create(Target.WindowsPlatform.Compiler, Target.WindowsPlatform.ToolChain, Platform, Target.WindowsPlatform.Architecture, Target.WindowsPlatform.CompilerVersion, Target.WindowsPlatform.ToolchainVersion, Target.WindowsPlatform.WindowsSdkVersion, null, Target.WindowsPlatform.bUseCPPWinRT, Target.WindowsPlatform.bAllowClangLinker, Logger);
		}

		/// <summary>
		/// Validate a target's settings
		/// </summary>
		[SupportedOSPlatform("windows")]
		public override void ValidateTarget(TargetRules Target)
		{
			if (Platform == UnrealTargetPlatform.Win64)
			{
				Target.WindowsPlatform.Architecture = Target.Architecture;// == UnrealArch.Default ? UnrealArch.X64 : Target.Architecture;

				// Add names of plugins here that are incompatible with arm64ec or arm64
				bool bCompilingForArm = !Target.Architecture.bIsX64;
				if (bCompilingForArm && Target.Name != "UnrealHeaderTool")
				{
					if (Target.WindowsPlatform.Architecture == UnrealArch.Arm64ec)
					{
						Target.DisablePlugins.AddRange(new string[]
						{
							"Reflex",
							"VirtualCamera", // WebRTC currently does not link properly
						});
					}

					Target.DisablePlugins.AddRange(new string[]
					{
						"OpenImageDenoise",
					});

					// VTune does not support ARM
					Target.GlobalDefinitions.Add("UE_EXTERNAL_PROFILING_ENABLED=0");
				}
			}

			// Disable Simplygon support if compiling against the NULL RHI.
			if (Target.GlobalDefinitions.Contains("USE_NULL_RHI=1"))
			{
				Target.bCompileCEF3 = false;
			}

			// If clang is selected for static analysis, switch compiler to clang and proceed
			// as normal.
			if (Target.StaticAnalyzer == StaticAnalyzer.Clang)
			{
				if (!Target.WindowsPlatform.Compiler.IsClang())
				{
					Target.WindowsPlatform.Compiler = WindowsCompiler.Clang;
				}
				Target.StaticAnalyzer = StaticAnalyzer.Default;

				// Clang static analysis requires non unity builds
				Target.bUseUnityBuild = false;
			}
			else if (Target.StaticAnalyzer != StaticAnalyzer.None &&
					 Target.StaticAnalyzerOutputType != StaticAnalyzerOutputType.Text)
			{
				Logger.LogInformation("Defaulting static analyzer output type to text");
			}

			if (Target.bUseAutoRTFMCompiler)
			{
				Target.WindowsPlatform.Compiler = WindowsCompiler.ClangRTFM;
			}

			// Set the compiler version if necessary
			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Default)
			{
				Target.WindowsPlatform.Compiler = GetDefaultCompiler(Target.ProjectFile, Target.WindowsPlatform.Architecture, Logger, !Platform.IsInGroup(UnrealPlatformGroup.Microsoft));
			}

			// Disable linking and ignore build outputs if we're using a static analyzer
			if (Target.StaticAnalyzer != StaticAnalyzer.None)
			{
				Target.bDisableLinking = true;
				Target.bIgnoreBuildOutputs = true;

				// Enable extended warnings when analyzing
				Target.WindowsPlatform.bVCExtendedWarningInfo = true;
			}

			// Disable PCHs for PVS studio analyzer.
			if (Target.StaticAnalyzer == StaticAnalyzer.PVSStudio)
			{
				Target.bUsePCHFiles = false;
			}

			// Disable chaining PCH files if not using clang
			if (!Target.WindowsPlatform.Compiler.IsClang())
			{
				Target.bChainPCHs = false;
			}

			// E&C support.
			if (Target.bSupportEditAndContinue || Target.bAdaptiveUnityEnablesEditAndContinue)
			{
				Target.bUseIncrementalLinking = true;
			}
			if (Target.bAdaptiveUnityEnablesEditAndContinue && !Target.bAdaptiveUnityDisablesPCH && !Target.bAdaptiveUnityCreatesDedicatedPCH)
			{
				throw new BuildException("bAdaptiveUnityEnablesEditAndContinue requires bAdaptiveUnityDisablesPCH or bAdaptiveUnityCreatesDedicatedPCH");
			}

			// for monolithic editor builds, add the PDBPAGESIZE option, (VS 16.11, VC toolchain 14.29.30133), but the pdb will be too large without this
			// some monolithic game builds could be too large as well, but they can be added in a .Target.cs with:
			// TargetRules.WindowsPlatform.PdbPageSize = 8192;
			if (!Target.WindowsPlatform.PdbPageSize.HasValue && Target.LinkType == TargetLinkType.Monolithic && Target.Type == TargetType.Editor)
			{
				Target.WindowsPlatform.PdbPageSize = 16384;
			}

			// If we're using PDB files and PCHs, the generated code needs to be compiled with the same options as the PCH.
			if ((Target.bUsePDBFiles || Target.bSupportEditAndContinue) && Target.bUsePCHFiles)
			{
				Target.bDisableDebugInfoForGeneratedCode = false;
			}

			Target.bCompileISPC = true;

			if (Platform == UnrealTargetPlatform.Win64 && !Target.Architecture.bIsX64)
			{
				Target.bCompileISPC = false; // The version of ISPC we currently use does not support Windows Aarch64
			}

			// Initialize the VC environment for the target, and set all the version numbers to the concrete values we chose
			Target.WindowsPlatform.Environment = CreateVCEnvironment(Target);

			// pull some things from it
			Target.WindowsPlatform.Compiler = Target.WindowsPlatform.Environment.Compiler;
			Target.WindowsPlatform.CompilerVersion = Target.WindowsPlatform.Environment.CompilerVersion.ToString();
			Target.WindowsPlatform.ToolChain = Target.WindowsPlatform.Environment.ToolChain;
			Target.WindowsPlatform.ToolchainVersion = Target.WindowsPlatform.Environment.ToolChainVersion.ToString();
			Target.WindowsPlatform.WindowsSdkVersion = Target.WindowsPlatform.Environment.WindowsSdkVersion.ToString();

			// Ensure we're using a recent enough version of Clang given the MSVC version
			if (Target.WindowsPlatform.Compiler.IsClang() && !MicrosoftPlatformSDK.IgnoreToolchainErrors)
			{
				VersionNumber ClangVersion = Target.WindowsPlatform.Compiler == WindowsCompiler.Intel ? MicrosoftPlatformSDK.GetClangVersionForIntelCompiler(Target.WindowsPlatform.Environment.CompilerPath) : Target.WindowsPlatform.Environment.CompilerVersion;
				VersionNumber MinimumClang = MicrosoftPlatformSDK.GetMinimumClangVersionForVcVersion(Target.WindowsPlatform.Environment.ToolChainVersion);
				if (ClangVersion < MinimumClang)
				{
					throw new BuildException("MSVC toolchain version {0} requires Clang compiler version {1} or later. The current Clang compiler version was detected as: {2}", Target.WindowsPlatform.Environment.ToolChainVersion, MinimumClang, ClangVersion);
				}
			}

			if (Target.WindowsPlatform.ToolchainVersionWarningLevel != WarningLevel.Off)
			{
				if (!MicrosoftPlatformSDK.IsPreferredVersion(Target.WindowsPlatform.Compiler, Target.WindowsPlatform.Environment.CompilerVersion))
				{
					VersionNumber preferred = MicrosoftPlatformSDK.GetLatestPreferredVersion(Target.WindowsPlatform.Compiler);
					MicrosoftPlatformSDK.DumpAllToolChainInstallations(Target.WindowsPlatform.Compiler, Target.Architecture, Logger);
					if (Target.WindowsPlatform.ToolchainVersionWarningLevel == WarningLevel.Error)
					{
						throw new BuildLogEventException("{Compiler} compiler version {Version} is not a preferred version. Please use the latest preferred version {PreferredVersion}", WindowsPlatform.GetCompilerName(Target.WindowsPlatform.Compiler), Target.WindowsPlatform.Environment.CompilerVersion, preferred);
					}
					Logger.LogInformation("{Compiler} compiler version {Version} is not a preferred version. Please use the latest preferred version {PreferredVersion}", WindowsPlatform.GetCompilerName(Target.WindowsPlatform.Compiler), Target.WindowsPlatform.Environment.CompilerVersion, preferred);
				}

				if (Target.WindowsPlatform.Compiler != Target.WindowsPlatform.ToolChain && !MicrosoftPlatformSDK.IsPreferredVersion(Target.WindowsPlatform.ToolChain, Target.WindowsPlatform.Environment.ToolChainVersion))
				{
					VersionNumber preferred = MicrosoftPlatformSDK.GetLatestPreferredVersion(Target.WindowsPlatform.ToolChain);
					MicrosoftPlatformSDK.DumpAllToolChainInstallations(Target.WindowsPlatform.ToolChain, Target.Architecture, Logger);
					if (Target.WindowsPlatform.ToolchainVersionWarningLevel == WarningLevel.Error)
					{
						throw new BuildLogEventException("{Toolchain} toolchain version {Version} is not a preferred version. Please use the latest preferred version {PreferredVersion}", WindowsPlatform.GetCompilerName(Target.WindowsPlatform.ToolChain), Target.WindowsPlatform.Environment.ToolChainVersion, preferred);
					}
					Logger.LogInformation("{Toolchain} toolchain version {Version} is not a preferred version. Please use a preferred toolchain such as {PreferredVersion}", WindowsPlatform.GetCompilerName(Target.WindowsPlatform.ToolChain), Target.WindowsPlatform.Environment.ToolChainVersion, preferred);
				}
			}

			//			@Todo: Still getting reports of frequent OOM issues with this enabled as of 15.7.
			//			// Enable fast PDB linking if we're on VS2017 15.7 or later. Previous versions have OOM issues with large projects.
			//			if(!Target.bFormalBuild && !Target.bUseFastPDBLinking.HasValue && Target.WindowsPlatform.Compiler.IsMSVC())
			//			{
			//				VersionNumber Version;
			//				DirectoryReference ToolChainDir;
			//				if(TryGetVCToolChainDir(Target.WindowsPlatform.Compiler, Target.WindowsPlatform.CompilerVersion, out Version, out ToolChainDir) && Version >= new VersionNumber(14, 14, 26316))
			//				{
			//					Target.bUseFastPDBLinking = true;
			//				}
			//			}
		}

		/// <summary>
		/// Gets the default compiler which should be used, if it's not set explicitly by the target, command line, or config file.
		/// </summary>
		/// <returns>The default compiler version</returns>
		internal static WindowsCompiler GetDefaultCompiler(FileReference? ProjectFile, UnrealArch Architecture, ILogger Logger, bool bSkipWarning = false)
		{
			// If there's no specific compiler set, try to pick the matching compiler for the selected IDE
			if (ProjectFileGeneratorSettings.Format != null)
			{
				foreach (ProjectFileFormat Format in ProjectFileGeneratorSettings.ParseFormatList(ProjectFileGeneratorSettings.Format, Logger))
				{
					if (Format == ProjectFileFormat.VisualStudio2022)
					{
						return WindowsCompiler.VisualStudio2022;
					}
				}
			}

			// Also check the default format for the Visual Studio project generator
			object? ProjectFormatObject;
			if (XmlConfig.TryGetValue(typeof(VCProjectFileSettings), "ProjectFileFormat", out ProjectFormatObject))
			{
				VCProjectFileFormat ProjectFormat = (VCProjectFileFormat)ProjectFormatObject;
				if (ProjectFormat == VCProjectFileFormat.VisualStudio2022)
				{
					return WindowsCompiler.VisualStudio2022;
				}
			}

			// Check the editor settings too
			ProjectFileFormat PreferredAccessor;
			if (ProjectFileGenerator.GetPreferredSourceCodeAccessor(ProjectFile, out PreferredAccessor))
			{
				if (PreferredAccessor == ProjectFileFormat.VisualStudio2022)
				{
					return WindowsCompiler.VisualStudio2022;
				}
			}

			// Second, default based on what's installed, test for 2022 first
			if (MicrosoftPlatformSDK.HasValidCompiler(WindowsCompiler.VisualStudio2022, Architecture, Logger))
			{
				return WindowsCompiler.VisualStudio2022;
			}

			if (!bSkipWarning)
			{
				// If we do have a Visual Studio installation, but we're missing just the C++ parts, warn about that.
				if (TryGetVSInstallDirs(WindowsCompiler.VisualStudio2022, Logger) != null)
				{
					string ToolSetWarning = Architecture == UnrealArch.X64 ?
						"MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)" :
						"MSVC v143 - VS 2022 C++ ARM64 build tools (Latest)";
					Logger.LogWarning("Visual Studio 2022 is installed, but is missing the C++ toolchain. Please verify that the \"{Component}\" component is selected in the Visual Studio 2022 installation options.", ToolSetWarning);
				}
				else
				{
					Logger.LogWarning("No Visual C++ installation was found. Please download and install Visual Studio 2022 with C++ components.");
				}
			}

			// Finally, default to VS2022 anyway
			return WindowsCompiler.VisualStudio2022;
		}

		/// <summary>
		/// Returns the human-readable name of the given compiler
		/// </summary>
		/// <param name="Compiler">The compiler value</param>
		/// <returns>Name of the compiler</returns>
		public static string GetCompilerName(WindowsCompiler Compiler)
		{
			return MicrosoftPlatformSDK.GetCompilerName(Compiler);
		}

		/// <summary>
		/// Get the first Visual Studio install directory for the given compiler version. Note that it is possible for the compiler toolchain to be installed without
		/// Visual Studio.
		/// </summary>
		/// <param name="Compiler">Version of the toolchain to look for.</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>True if the directory was found, false otherwise.</returns>
		public static IEnumerable<DirectoryReference>? TryGetVSInstallDirs(WindowsCompiler Compiler, ILogger Logger)
		{
			List<VisualStudioInstallation> Installations = MicrosoftPlatformSDK.FindVisualStudioInstallations(Compiler, Logger);
			if (Installations.Count == 0)
			{
				return null;
			}

			return Installations.Select(x => x.BaseDir);
		}

		/// <summary>
		/// Determines if a given compiler is installed and valid
		/// </summary>
		/// <param name="Compiler">Compiler to check for</param>
		/// <param name="Architecture">Architecture the compiler must support</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>True if the given compiler is installed and valid</returns>
		public static bool HasCompiler(WindowsCompiler Compiler, UnrealArch Architecture, ILogger Logger)
		{
			return MicrosoftPlatformSDK.HasCompiler(Compiler, Architecture, Logger);
		}

		/// <summary>
		/// Determines the directory containing the MSVC toolchain
		/// </summary>
		/// <param name="Compiler">Major version of the compiler to use</param>
		/// <param name="CompilerVersion">The minimum compiler version to use</param>
		/// <param name="Architecture">Architecture that is required</param>
		/// <param name="Logger">Logger for output</param>
		/// <param name="OutToolChainVersion">Receives the chosen toolchain version</param>
		/// <param name="OutToolChainDir">Receives the directory containing the toolchain</param>
		/// <param name="OutRedistDir">Receives the optional directory containing redistributable components</param>
		/// <returns>True if the toolchain directory was found correctly</returns>
		public static bool TryGetToolChainDir(WindowsCompiler Compiler, string? CompilerVersion, UnrealArch Architecture, ILogger Logger, [NotNullWhen(true)] out VersionNumber? OutToolChainVersion, [NotNullWhen(true)] out DirectoryReference? OutToolChainDir, out DirectoryReference? OutRedistDir)
		{
			return MicrosoftPlatformSDK.TryGetToolChainDir(Compiler, CompilerVersion, Architecture, Logger, out OutToolChainVersion, out OutToolChainDir, out OutRedistDir);
		}

		public static string GetArchitectureName(UnrealArch arch)
		{
			return arch.ToString();
		}

		/// <summary>
		/// Determines if a directory contains a valid DIA SDK
		/// </summary>
		/// <param name="DiaSdkDir">The directory to check</param>
		/// <returns>True if it contains a valid DIA SDK</returns>
		static bool IsValidDiaSdkDir(DirectoryReference DiaSdkDir)
		{
			return FileReference.Exists(FileReference.Combine(DiaSdkDir, "bin", "amd64", "msdia140.dll"));
		}

		[SupportedOSPlatform("windows")]
		public static bool TryGetWindowsSdkDir(string? DesiredVersion, ILogger Logger, [NotNullWhen(true)] out VersionNumber? OutSdkVersion, [NotNullWhen(true)] out DirectoryReference? OutSdkDir)
		{
			return MicrosoftPlatformSDK.TryGetWindowsSdkDir(DesiredVersion, Logger, out OutSdkVersion, out OutSdkDir);
		}

		/// <summary>
		/// Gets the platform name that should be used.
		/// </summary>
		public override string GetPlatformName()
		{
			return "Windows";
		}

		/// <summary>
		/// If this platform can be compiled with SN-DBS
		/// </summary>
		public override bool CanUseSNDBS()
		{
			return true;
		}

		/// <summary>
		/// If this platform can be compiled with FASTBuild
		/// </summary>
		public override bool CanUseFASTBuild()
		{
			return true;
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UnrealEditor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public override bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".exe")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dll")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dll.response")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".dll.rsp")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".lib")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".pdb")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".full.pdb")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".exp")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".obj")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".map")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".objpaths")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".natvis")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".natstepfilter");
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binrary type being built</param>
		/// <returns>string    The binary extenstion (ie 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".dll";
				case UEBuildBinaryType.Executable:
					return ".exe";
				case UEBuildBinaryType.StaticLibrary:
					return ".lib";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extensions to use for debug info for the given binary type
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string[]    The debug info extensions (i.e. 'pdb')</returns>
		public override string[] GetDebugInfoExtensions(ReadOnlyTargetRules Target, UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
				case UEBuildBinaryType.Executable:
					return new string[] { ".pdb" };
			}
			return new string[] { };
		}

		public override bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectPath)
		{
			// check the base settings
			return base.HasDefaultBuildConfig(Platform, ProjectPath);
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
		}

		/// <summary>
		/// Gets the application icon for a given project
		/// </summary>
		/// <param name="ProjectFile">The project file</param>
		/// <returns>The icon to use for this project</returns>
		public static FileReference GetWindowsApplicationIcon(FileReference? ProjectFile)
		{
			// Check if there's a custom icon
			if (ProjectFile != null)
			{
				FileReference IconFile = FileReference.Combine(ProjectFile.Directory, "Build", "Windows", "Application.ico");
				if (FileReference.Exists(IconFile))
				{
					return IconFile;
				}
			}

			// Otherwise use the default
			return FileReference.Combine(Unreal.EngineDirectory, "Build", "Windows", "Resources", "Default.ico");
		}

		/// <summary>
		/// Gets the application icon for a given project
		/// </summary>
		/// <param name="ProjectFile">The project file</param>
		/// <returns>The icon to use for this project</returns>
		public virtual FileReference GetApplicationIcon(FileReference ProjectFile)
		{
			return GetWindowsApplicationIcon(ProjectFile);
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
			bool bBuildShaderFormats = Target.bForceBuildShaderFormats;

			if (!Target.bBuildRequiresCookedData && Target.Type != TargetType.Program)
			{
				if (ModuleName == "TargetPlatform")
				{
					bBuildShaderFormats = true;
				}
			}

			// allow standalone tools to use target platform modules, without needing Engine
			if (ModuleName == "TargetPlatform")
			{
				if (Target.bForceBuildTargetPlatforms)
				{
					Rules.DynamicallyLoadedModuleNames.Add("WindowsTargetPlatform");
				}

				if (bBuildShaderFormats)
				{
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatD3D");
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatVectorVM");

					Rules.DynamicallyLoadedModuleNames.Remove("VulkanRHI");
					Rules.DynamicallyLoadedModuleNames.Add("VulkanShaderFormat");
				}
			}

			// Delay-load D3D12 so we can use the latest features and still run on downlevel versions of the OS
			Rules.PublicDelayLoadDLLs.Add("d3d12.dll");
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			// @todo Remove this hack to work around broken includes
			CompileEnvironment.Definitions.Add("NDIS_MINIPORT_MAJOR_VERSION=0");

			CompileEnvironment.Definitions.Add("WIN32=1");

			CompileEnvironment.Definitions.Add(String.Format("_WIN32_WINNT=0x{0:X4}", Target.WindowsPlatform.TargetWindowsVersion));
			CompileEnvironment.Definitions.Add(String.Format("WINVER=0x{0:X4}", Target.WindowsPlatform.TargetWindowsVersion));

			if (Target.WindowsPlatform.TargetWindowsMinorVersion != null)
			{
				CompileEnvironment.Definitions.Add(String.Format("NTDDI_VERSION=0x{0:X8}", Target.WindowsPlatform.TargetWindowsMinorVersion));
			}

			CompileEnvironment.Definitions.Add("PLATFORM_WINDOWS=1");
			CompileEnvironment.Definitions.Add("PLATFORM_MICROSOFT=1");

			string? OverridePlatformHeaderName = GetOverridePlatformHeaderName();
			if (!String.IsNullOrEmpty(OverridePlatformHeaderName))
			{
				CompileEnvironment.Definitions.Add(String.Format("OVERRIDE_PLATFORM_HEADER_NAME={0}", OverridePlatformHeaderName));
			}

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows) && Target.WindowsPlatform.bEnableRayTracing && Target.Type != TargetType.Server)
			{
				CompileEnvironment.Definitions.Add("RHI_RAYTRACING=1");
			}

			// Explicitly exclude the MS C++ runtime libraries we're not using, to ensure other libraries we link with use the same
			// runtime library as the engine.
			bool bUseDebugCRT = Target.Configuration == UnrealTargetConfiguration.Debug && Target.bDebugBuildsActuallyUseDebugCRT;
			if (!Target.bUseStaticCRT || bUseDebugCRT)
			{
				LinkEnvironment.ExcludedLibraries.Add("LIBCMT");
				LinkEnvironment.ExcludedLibraries.Add("LIBCPMT");
			}
			if (!Target.bUseStaticCRT || !bUseDebugCRT)
			{
				LinkEnvironment.ExcludedLibraries.Add("LIBCMTD");
				LinkEnvironment.ExcludedLibraries.Add("LIBCPMTD");
			}
			if (Target.bUseStaticCRT || bUseDebugCRT)
			{
				LinkEnvironment.ExcludedLibraries.Add("MSVCRT");
				LinkEnvironment.ExcludedLibraries.Add("MSVCPRT");
			}
			if (Target.bUseStaticCRT || !bUseDebugCRT)
			{
				LinkEnvironment.ExcludedLibraries.Add("MSVCRTD");
				LinkEnvironment.ExcludedLibraries.Add("MSVCPRTD");
			}
			LinkEnvironment.ExcludedLibraries.Add("LIBC");
			LinkEnvironment.ExcludedLibraries.Add("LIBCP");
			LinkEnvironment.ExcludedLibraries.Add("LIBCD");
			LinkEnvironment.ExcludedLibraries.Add("LIBCPD");

			//@todo ATL: Currently, only VSAccessor requires ATL (which is only used in editor builds)
			// When compiling games, we do not want to include ATL - and we can't when compiling games
			// made with Launcher build due to VS 2012 Express not including ATL.
			// If more modules end up requiring ATL, this should be refactored into a BuildTarget flag (bNeedsATL)
			// that is set by the modules the target includes to allow for easier tracking.
			// Alternatively, if VSAccessor is modified to not require ATL than we should always exclude the libraries.
			if (Target.LinkType == TargetLinkType.Monolithic &&
				(Target.Type == TargetType.Game || Target.Type == TargetType.Client || Target.Type == TargetType.Server))
			{
				LinkEnvironment.ExcludedLibraries.Add("atl");
				LinkEnvironment.ExcludedLibraries.Add("atls");
				LinkEnvironment.ExcludedLibraries.Add("atlsd");
				LinkEnvironment.ExcludedLibraries.Add("atlsn");
				LinkEnvironment.ExcludedLibraries.Add("atlsnd");
			}

			// Add the library used for the delayed loading of DLLs.
			LinkEnvironment.SystemLibraries.Add("delayimp.lib");

			//@todo - remove once FB implementation uses Http module
			if (Target.bCompileAgainstEngine)
			{
				// link against wininet (used by FBX and Facebook)
				LinkEnvironment.SystemLibraries.Add("wininet.lib");
			}

			// Compile and link with Win32 API libraries.
			LinkEnvironment.SystemLibraries.Add("rpcrt4.lib");
			//LinkEnvironment.AdditionalLibraries.Add("wsock32.lib");
			LinkEnvironment.SystemLibraries.Add("ws2_32.lib");
			LinkEnvironment.SystemLibraries.Add("dbghelp.lib");
			LinkEnvironment.SystemLibraries.Add("comctl32.lib");
			LinkEnvironment.SystemLibraries.Add("Winmm.lib");
			LinkEnvironment.SystemLibraries.Add("kernel32.lib");
			LinkEnvironment.SystemLibraries.Add("user32.lib");
			LinkEnvironment.SystemLibraries.Add("gdi32.lib");
			LinkEnvironment.SystemLibraries.Add("winspool.lib");
			LinkEnvironment.SystemLibraries.Add("comdlg32.lib");
			LinkEnvironment.SystemLibraries.Add("advapi32.lib");
			LinkEnvironment.SystemLibraries.Add("shell32.lib");
			LinkEnvironment.SystemLibraries.Add("ole32.lib");
			LinkEnvironment.SystemLibraries.Add("oleaut32.lib");
			LinkEnvironment.SystemLibraries.Add("uuid.lib");
			LinkEnvironment.SystemLibraries.Add("odbc32.lib");
			LinkEnvironment.SystemLibraries.Add("odbccp32.lib");
			LinkEnvironment.SystemLibraries.Add("netapi32.lib");
			LinkEnvironment.SystemLibraries.Add("iphlpapi.lib");
			LinkEnvironment.SystemLibraries.Add("setupapi.lib"); //  Required for access monitor device enumeration
			LinkEnvironment.SystemLibraries.Add("synchronization.lib"); // Required for WaitOnAddress and WakeByAddressSingle

			// Windows 7 Desktop Windows Manager API for Slate Windows Compliance
			LinkEnvironment.SystemLibraries.Add("dwmapi.lib");

			// IME
			LinkEnvironment.SystemLibraries.Add("imm32.lib");

			// For 64-bit builds, we'll forcibly ignore a linker warning with DirectInput.  This is
			// Microsoft's recommended solution as they don't have a fixed .lib for us.
			LinkEnvironment.AdditionalArguments += " /ignore:4078";

			// Set up default stack size
			LinkEnvironment.DefaultStackSize = Target.WindowsPlatform.DefaultStackSize;
			LinkEnvironment.DefaultStackSizeCommit = Target.WindowsPlatform.DefaultStackSizeCommit;

			LinkEnvironment.ModuleDefinitionFile = Target.WindowsPlatform.ModuleDefinitionFile;

			if (Target.bPGOOptimize || Target.bPGOProfile)
			{
				// Win64 PGO folder is Windows, the rest match the platform name
				string PGOPlatform = Target.Platform == UnrealTargetPlatform.Win64 ? "Windows" : Target.Platform.ToString();

				CompileEnvironment.PGODirectory = DirectoryReference.Combine(Target.ProjectFile?.Directory ?? Unreal.WritableEngineDirectory, "Platforms", PGOPlatform, "Build", "PGO").FullName;
				CompileEnvironment.PGOFilenamePrefix = String.Format("{0}-{1}-{2}", Target.Name, Target.Platform, Target.Configuration);

				LinkEnvironment.PGODirectory = CompileEnvironment.PGODirectory;
				LinkEnvironment.PGOFilenamePrefix = CompileEnvironment.PGOFilenamePrefix;
			}

			CompileEnvironment.Definitions.Add("WINDOWS_MAX_NUM_TLS_SLOTS=" + Target.WindowsPlatform.MaxNumTlsSlots.ToString());
			CompileEnvironment.Definitions.Add("WINDOWS_MAX_NUM_THREADS_WITH_TLS_SLOTS=" + Target.WindowsPlatform.MaxNumThreadsWithTlsSlots.ToString());
		}

		/// <summary>
		/// Setup the configuration environment for building
		/// </summary>
		/// <param name="Target"> The target being built</param>
		/// <param name="GlobalCompileEnvironment">The global compile environment</param>
		/// <param name="GlobalLinkEnvironment">The global link environment</param>
		public override void SetUpConfigurationEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			base.SetUpConfigurationEnvironment(Target, GlobalCompileEnvironment, GlobalLinkEnvironment);

			// NOTE: Even when debug info is turned off, we currently force the linker to generate debug info
			//       anyway on Visual C++ platforms.  This will cause a PDB file to be generated with symbols
			//       for most of the classes and function/method names, so that crashes still yield somewhat
			//       useful call stacks, even though compiler-generate debug info may be disabled.  This gives
			//       us much of the build-time savings of fully-disabled debug info, without giving up call
			//       data completely.
			GlobalLinkEnvironment.bCreateDebugInfo = true;
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
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
			VCToolChain toolchain = new VCToolChain(Target, Logger);
			if (Target.StaticAnalyzer == StaticAnalyzer.PVSStudio)
			{
				return new PVSToolChain(Target, toolchain, Logger);
			}
			return toolchain;
		}

		/// <summary>
		/// Allows the platform to return various build metadata that is not tracked by other means. If the returned string changes, the makefile will be invalidated.
		/// </summary>
		/// <param name="ProjectFile">The project file being built</param>
		/// <param name="Metadata">String builder to contain build metadata</param>
		public override void GetExternalBuildMetadata(FileReference? ProjectFile, StringBuilder Metadata)
		{
			base.GetExternalBuildMetadata(ProjectFile, Metadata);

			if (ProjectFile != null)
			{
				Metadata.AppendLine("ICON: {0}", GetApplicationIcon(ProjectFile));
			}
		}

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Receipt">Receipt for the target being deployed</param>
		public override void Deploy(TargetReceipt Receipt)
		{
			new UEDeployWindows(Logger).PrepTargetForDeployment(Receipt);
		}

		/// <summary>
		/// Allows the platform header name to be overridden to differ from the platform name.
		/// </summary>
		protected virtual string? GetOverridePlatformHeaderName()
		{
			// Both Win32 and Win64 use Windows headers, so we enforce that here.
			return GetPlatformName();
		}
	}

	class UEDeployWindows : UEBuildDeploy
	{
		public UEDeployWindows(ILogger InLogger)
			: base(InLogger)
		{
		}

		public override bool PrepTargetForDeployment(TargetReceipt Receipt)
		{
			return base.PrepTargetForDeployment(Receipt);
		}
	}

	class WindowsPlatformFactory : UEBuildPlatformFactory
	{
		public override UnrealTargetPlatform TargetPlatform => UnrealTargetPlatform.Win64;

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms(ILogger Logger)
		{
			MicrosoftPlatformSDK SDK = new MicrosoftPlatformSDK(Logger);

			// Register this build platform for Win64 (no more Win32)
			UEBuildPlatform.RegisterBuildPlatform(new WindowsPlatform(UnrealTargetPlatform.Win64, SDK, Logger), Logger);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win64, UnrealPlatformGroup.Windows);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win64, UnrealPlatformGroup.Microsoft);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Win64, UnrealPlatformGroup.Desktop);
		}
	}
}
