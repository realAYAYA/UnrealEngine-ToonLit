// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool
{

	/// <summary>
	/// Compiler configuration. This controls whether to use define debug macros and other compiler settings. Note that optimization level should be based on the bOptimizeCode variable rather than
	/// this setting, so it can be modified on a per-module basis without introducing an incompatibility between object files or PCHs.
	/// </summary>
	enum CppConfiguration
	{
		Debug,
		Development,
		Shipping
	}

	/// <summary>
	/// Specifies which language standard to use. This enum should be kept in order, so that toolchains can check whether the requested setting is >= values that they support.
	/// </summary>
	public enum CppStandardVersion
	{
		/// <summary>
		/// Supports C++14
		/// </summary>
		Cpp14,

		/// <summary>
		/// Supports C++17
		/// </summary>
		Cpp17,

		/// <summary>
		/// Supports C++20
		/// </summary>
		Cpp20,
		
		/// <summary>
		/// Latest standard supported by the compiler
		/// </summary>
		Latest,
		
		/// <summary>
		/// Use the default standard version
		/// </summary>
		Default = Cpp17,
	}
	
	/// <summary>
	/// Specifies which C language standard to use. This enum should be kept in order, so that toolchains can check whether the requested setting is >= values that they support.
	/// </summary>
	public enum CStandardVersion
	{
		/// <summary>
		/// Use the default standard version
		/// </summary>
		Default,

		/// <summary>
		/// Supports C89
		/// </summary>
		C89,

		/// <summary>
		/// Supports C99
		/// </summary>
		C99,

		/// <summary>
		/// Supports C11
		/// </summary>
		C11,

		/// <summary>
		/// Supports C17
		/// </summary>
		C17,

		/// <summary>
		/// Latest standard supported by the compiler
		/// </summary>
		Latest,
	}

	/// <summary>
	/// The optimization level that may be compilation targets for C# files.
	/// </summary>
	enum CSharpTargetConfiguration
	{
		Debug,
		Development,
	}

	/// <summary>
	/// The possible interactions between a precompiled header and a C++ file being compiled.
	/// </summary>
	enum PrecompiledHeaderAction
	{
		None,
		Include,
		Create
	}

	/// <summary>
	/// Encapsulates the compilation output of compiling a set of C++ files.
	/// </summary>
	class CPPOutput
	{
		public List<FileItem> ObjectFiles = new List<FileItem>();
		public List<FileItem> CompiledModuleInterfaces = new List<FileItem>();
		public List<FileItem> GeneratedHeaderFiles = new List<FileItem>();
		public FileItem? PrecompiledHeaderFile = null;
	}

	/// <summary>
	/// Encapsulates the environment that a C++ file is compiled in.
	/// </summary>
	class CppCompileEnvironment
	{
		/// <summary>
		/// The platform to be compiled/linked for.
		/// </summary>
		public readonly UnrealTargetPlatform Platform;

		/// <summary>
		/// The configuration to be compiled/linked for.
		/// </summary>
		public readonly CppConfiguration Configuration;

		/// <summary>
		/// The architecture that is being compiled/linked (empty string by default)
		/// </summary>
		public readonly string Architecture;

		/// <summary>
		/// Cache of source file metadata
		/// </summary>
		public readonly SourceFileMetadataCache MetadataCache;

		/// <summary>
		/// Templates for shared precompiled headers
		/// </summary>
		public readonly List<PrecompiledHeaderTemplate> SharedPCHs;

		/// <summary>
		/// The name of the header file which is precompiled.
		/// </summary>
		public FileReference? PrecompiledHeaderIncludeFilename = null;

		/// <summary>
		/// Whether the compilation should create, use, or do nothing with the precompiled header.
		/// </summary>
		public PrecompiledHeaderAction PrecompiledHeaderAction = PrecompiledHeaderAction.None;

		/// <summary>
		/// Whether artifacts from this compile are shared with other targets. If so, we should not apply any target-wide modifications to the compile environment.
		/// </summary>
		public bool bUseSharedBuildEnvironment;

		/// <summary>
		/// Use run time type information
		/// </summary>
		public bool bUseRTTI = false;

		/// <summary>
		/// Enable inlining.
		/// </summary>
		public bool bUseInlining = false;

		/// <summary>
		/// Whether to compile ISPC files.
		/// </summary>
		public bool bCompileISPC = false;

		/// <summary>
		/// Direct the compiler to generate AVX instructions wherever SSE or AVX intrinsics are used.
		/// Note that by enabling this you are changing the minspec for the PC platform, and the resultant executable will crash on machines without AVX support.
		/// </summary>
		public bool bUseAVX = false;

		/// <summary>
		/// Enable buffer security checks.   This should usually be enabled as it prevents severe security risks.
		/// </summary>
		public bool bEnableBufferSecurityChecks = true;

		/// <summary>
		/// If unity builds are enabled this can be used to override if this specific module will build using Unity.
		/// This is set using the per module configurations in BuildConfiguration.
		/// </summary>
		public bool bUseUnity = false;

		/// <summary>
		/// The number of source files in this module before unity build will be activated for that module.  If set to
		/// anything besides -1, will override the default setting which is controlled by MinGameModuleSourceFilesForUnityBuild
		/// </summary>
		public int MinSourceFilesForUnityBuildOverride = 0;

		/// <summary>
		/// The minimum number of files that must use a pre-compiled header before it will be created and used.
		/// </summary>
		public int MinFilesUsingPrecompiledHeaderOverride = 0;

		/// <summary>
		/// Module uses a #import so must be built locally when compiling with SN-DBS
		/// </summary>
		public bool bBuildLocallyWithSNDBS = false;

		/// <summary>
		/// Whether to retain frame pointers
		/// </summary>
		public bool bRetainFramePointers = true;

		/// <summary>
		/// Enable exception handling
		/// </summary>
		public bool bEnableExceptions = false;

		/// <summary>
		/// Enable objective C exception handling
		/// </summary>
		public bool bEnableObjCExceptions = false;

		/// <summary>
		/// How to treat any warnings in the code
		/// </summary>
		public WarningLevel DefaultWarningLevel = WarningLevel.Warning;

		/// <summary>
		/// Whether to warn about deprecated variables
		/// </summary>
		public WarningLevel DeprecationWarningLevel = WarningLevel.Warning;

		/// <summary>
		/// Whether to warn about the use of shadow variables
		/// </summary>
		public WarningLevel ShadowVariableWarningLevel = WarningLevel.Warning;

		/// <summary>
		/// How to treat unsafe implicit type cast warnings (e.g., double->float or int64->int32)
		/// </summary>
		public WarningLevel UnsafeTypeCastWarningLevel = WarningLevel.Off;

		/// <summary>
		/// Whether to warn about the use of undefined identifiers in #if expressions
		/// </summary>
		public bool bEnableUndefinedIdentifierWarnings = true;

		/// <summary>
		/// Whether to treat undefined identifier warnings as errors.
		/// </summary>
		public bool bUndefinedIdentifierWarningsAsErrors = false;

		/// <summary>
		/// Whether to treat all warnings as errors
		/// </summary>
		public bool bWarningsAsErrors = false;

		/// <summary>
		/// Whether to disable all static analysis - Clang, MSVC, PVS-Studio.
		/// </summary>
		public bool bDisableStaticAnalysis = false;

		/// <summary>
		/// The static analyzer checkers that should be enabled rather than the defaults. This is only supported for Clang.
		/// </summary>
		public HashSet<string> StaticAnalyzerCheckers = new HashSet<string>();

		/// <summary>
		/// The static analyzer default checkers that should be disabled. Unused if StaticAnalyzerCheckers is populated. This is only supported for Clang.
		/// </summary>
		public HashSet<string> StaticAnalyzerDisabledCheckers = new HashSet<string>();

		/// <summary>
		/// The static analyzer non-default checkers that should be enabled. Unused if StaticAnalyzerCheckers is populated. This is only supported for Clang.
		/// </summary>
		public HashSet<string> StaticAnalyzerAdditionalCheckers = new HashSet<string>();

		/// <summary>
		/// True if compiler optimizations should be enabled. This setting is distinct from the configuration (see CPPTargetConfiguration).
		/// </summary>
		public bool bOptimizeCode = false;

		/// <summary>
		/// Allows to fine tune optimizations level for speed and\or code size
		/// </summary>
		public OptimizationMode OptimizationLevel = OptimizationMode.Speed;

		/// <summary>
		/// True if debug info should be created.
		/// </summary>
		public bool bCreateDebugInfo = true;

		/// <summary>
		/// True if we're compiling .cpp files that will go into a library (.lib file)
		/// </summary>
		public bool bIsBuildingLibrary = false;

		/// <summary>
		/// True if we're compiling a DLL
		/// </summary>
		public bool bIsBuildingDLL = false;

		/// <summary>
		/// Whether we should compile using the statically-linked CRT. This is not widely supported for the whole engine, but is required for programs that need to run without dependencies.
		/// </summary>
		public bool bUseStaticCRT = false;

		/// <summary>
		/// Whether to use the debug CRT in debug configurations
		/// </summary>
		public bool bUseDebugCRT = false;

		/// <summary>
		/// Whether to omit frame pointers or not. Disabling is useful for e.g. memory profiling on the PC
		/// </summary>
		public bool bOmitFramePointers = true;

		/// <summary>
		/// Whether we should compile with support for OS X 10.9 Mavericks. Used for some tools that we need to be compatible with this version of OS X.
		/// </summary>
		public bool bEnableOSX109Support = false;

		/// <summary>
		/// Whether PDB files should be used for Visual C++ builds.
		/// </summary>
		public bool bUsePDBFiles = false;

		/// <summary>
		/// Whether to just preprocess source files
		/// </summary>
		public bool bPreprocessOnly = false;

		/// <summary>
		/// Whether to support edit and continue.  Only works on Microsoft compilers in 32-bit compiles.
		/// </summary>
		public bool bSupportEditAndContinue;

		/// <summary>
		/// Whether to use incremental linking or not.
		/// </summary>
		public bool bUseIncrementalLinking;

		/// <summary>
		/// Whether to allow the use of LTCG (link time code generation) 
		/// </summary>
		public bool bAllowLTCG;

        /// <summary>
        /// Whether to enable Profile Guided Optimization (PGO) instrumentation in this build.
        /// </summary>
        public bool bPGOProfile;
        
        /// <summary>
        /// Whether to optimize this build with Profile Guided Optimization (PGO).
        /// </summary>
        public bool bPGOOptimize;

        /// <summary>
        /// Platform specific directory where PGO profiling data is stored.
        /// </summary>
        public string? PGODirectory;

        /// <summary>
        /// Platform specific filename where PGO profiling data is saved.
        /// </summary>
        public string? PGOFilenamePrefix;

		/// <summary>
		/// Whether to log detailed timing info from the compiler
		/// </summary>
		public bool bPrintTimingInfo;

		/// <summary>
		/// Whether to output a dependencies file along with the output build products
		/// </summary>
		public bool bGenerateDependenciesFile = true;

		/// <summary>
		/// When enabled, allows XGE to compile pre-compiled header files on remote machines.  Otherwise, PCHs are always generated locally.
		/// </summary>
		public bool bAllowRemotelyCompiledPCHs = false;

		/// <summary>
		/// Ordered list of include paths for the module
		/// </summary>
		public HashSet<DirectoryReference> UserIncludePaths;

		/// <summary>
		/// The include paths where changes to contained files won't cause dependent C++ source files to
		/// be recompiled, unless BuildConfiguration.bCheckSystemHeadersForModification==true.
		/// </summary>
		public HashSet<DirectoryReference> SystemIncludePaths;

		/// <summary>
		/// List of paths to search for compiled module interface (*.ifc) files
		/// </summary>
		public HashSet<DirectoryReference> ModuleInterfacePaths;

		/// <summary>
		/// Whether headers in system paths should be checked for modification when determining outdated actions.
		/// </summary>
		public bool bCheckSystemHeadersForModification;

		/// <summary>
		/// List of header files to force include
		/// </summary>
		public List<FileItem> ForceIncludeFiles = new List<FileItem>();

		/// <summary>
		/// List of files that need to be up to date before compile can proceed
		/// </summary>
		public List<FileItem> AdditionalPrerequisites = new List<FileItem>();

		/// <summary>
		/// A dictionary of the source file items and the inlined gen.cpp files contained in it
		/// </summary>
		public Dictionary<FileItem, List<FileItem>> FileInlineGenCPPMap = new();

		/// <summary>
		/// The C++ preprocessor definitions to use.
		/// </summary>
		public List<string> Definitions = new List<string>();

		/// <summary>
		/// Additional arguments to pass to the compiler.
		/// </summary>
		public string AdditionalArguments = "";

		/// <summary>
		/// A list of additional frameworks whose include paths are needed.
		/// </summary>
		public List<UEBuildFramework> AdditionalFrameworks = new List<UEBuildFramework>();

		/// <summary>
		/// The file containing the precompiled header data.
		/// </summary>
		public FileItem? PrecompiledHeaderFile = null;

		/// <summary>
		/// Whether or not UHT is being built
		/// </summary>
		public bool bHackHeaderGenerator;

		/// <summary>
		/// Whether to hide symbols by default
		/// </summary>
		public bool bHideSymbolsByDefault = true;

		/// <summary>
		/// Which C++ standard to support. May not be compatible with all platforms.
		/// </summary>
		public CppStandardVersion CppStandard = CppStandardVersion.Default;
		
		/// <summary>
		/// Which C standard to support. May not be compatible with all platforms.
		/// </summary>
		public CStandardVersion CStandard = CStandardVersion.Default;

		/// <summary>
		/// The amount of the stack usage to report static analysis warnings.
		/// </summary>
		public int AnalyzeStackSizeWarning = 300000;

		/// <summary>
		/// Enable C++ coroutine support. 
		/// For MSVC, adds "/await:strict" to the command line. Program should #include &lt;coroutine&gt;
		/// For Clang, adds "-fcoroutines-ts" to the command line. Program should #include &lt;experimental/coroutine&gt; (not supported in every clang toolchain)
		/// </summary>
		public bool bEnableCoroutines = false;

		/// <summary>
		/// What version of include order specified by the module rules. Used to determine shared PCH variants.
		/// </summary>
		public EngineIncludeOrderVersion IncludeOrderVersion = EngineIncludeOrderVersion.Latest;

		/// <summary>
		/// Set flags for determinstic compiles (experimental).
		/// </summary>
		public bool bDeterministic = false;

		/// <summary>
		/// Directory where to put crash report files for platforms that support it
		/// </summary>
		public string? CrashDiagnosticDirectory;

		/// <summary>
		/// Default constructor.
		/// </summary>
        public CppCompileEnvironment(UnrealTargetPlatform Platform, CppConfiguration Configuration, string Architecture, SourceFileMetadataCache MetadataCache)
		{
			this.Platform = Platform;
			this.Configuration = Configuration;
			this.Architecture = Architecture;
			this.MetadataCache = MetadataCache;
			this.SharedPCHs = new List<PrecompiledHeaderTemplate>();
			this.UserIncludePaths = new HashSet<DirectoryReference>();
			this.SystemIncludePaths = new HashSet<DirectoryReference>();
			this.ModuleInterfacePaths = new HashSet<DirectoryReference>();
		}

		/// <summary>
		/// Copy constructor.
		/// </summary>
		/// <param name="Other">Environment to copy settings from</param>
		public CppCompileEnvironment(CppCompileEnvironment Other)
		{
			Platform = Other.Platform;
			Configuration = Other.Configuration;
			Architecture = Other.Architecture;
			MetadataCache = Other.MetadataCache;
			SharedPCHs = Other.SharedPCHs;
			PrecompiledHeaderIncludeFilename = Other.PrecompiledHeaderIncludeFilename;
			PrecompiledHeaderAction = Other.PrecompiledHeaderAction;
			bUseSharedBuildEnvironment = Other.bUseSharedBuildEnvironment;
			bUseRTTI = Other.bUseRTTI;
			bUseInlining = Other.bUseInlining;
			bCompileISPC = Other.bCompileISPC;
			bUseAVX = Other.bUseAVX;
			bUseUnity = Other.bUseUnity;
			MinSourceFilesForUnityBuildOverride = Other.MinSourceFilesForUnityBuildOverride;
			MinFilesUsingPrecompiledHeaderOverride = Other.MinFilesUsingPrecompiledHeaderOverride;
			bBuildLocallyWithSNDBS = Other.bBuildLocallyWithSNDBS;
			bRetainFramePointers = Other.bRetainFramePointers;
			bEnableExceptions = Other.bEnableExceptions;
			bEnableObjCExceptions = Other.bEnableObjCExceptions;
			DefaultWarningLevel = Other.DefaultWarningLevel;
			DeprecationWarningLevel = Other.DeprecationWarningLevel;
			ShadowVariableWarningLevel = Other.ShadowVariableWarningLevel;
			UnsafeTypeCastWarningLevel = Other.UnsafeTypeCastWarningLevel;
			bUndefinedIdentifierWarningsAsErrors = Other.bUndefinedIdentifierWarningsAsErrors;
			bEnableUndefinedIdentifierWarnings = Other.bEnableUndefinedIdentifierWarnings;
			bWarningsAsErrors = Other.bWarningsAsErrors;
			bDisableStaticAnalysis = Other.bDisableStaticAnalysis;
			StaticAnalyzerCheckers = new HashSet<string>(Other.StaticAnalyzerCheckers);
			StaticAnalyzerDisabledCheckers = new HashSet<string>(Other.StaticAnalyzerDisabledCheckers);
			StaticAnalyzerAdditionalCheckers = new HashSet<string>(Other.StaticAnalyzerAdditionalCheckers);
			bOptimizeCode = Other.bOptimizeCode;
			OptimizationLevel = Other.OptimizationLevel;
			bCreateDebugInfo = Other.bCreateDebugInfo;
			bIsBuildingLibrary = Other.bIsBuildingLibrary;
			bIsBuildingDLL = Other.bIsBuildingDLL;
			bUseStaticCRT = Other.bUseStaticCRT;
			bUseDebugCRT = Other.bUseDebugCRT;
			bOmitFramePointers = Other.bOmitFramePointers;
			bEnableOSX109Support = Other.bEnableOSX109Support;
			bUsePDBFiles = Other.bUsePDBFiles;
			bPreprocessOnly = Other.bPreprocessOnly;
			bSupportEditAndContinue = Other.bSupportEditAndContinue;
			bUseIncrementalLinking = Other.bUseIncrementalLinking;
			bAllowLTCG = Other.bAllowLTCG;
			bPGOOptimize = Other.bPGOOptimize;
			bPGOProfile = Other.bPGOProfile;
			PGOFilenamePrefix = Other.PGOFilenamePrefix;
			PGODirectory = Other.PGODirectory;
			bPrintTimingInfo = Other.bPrintTimingInfo;
			bGenerateDependenciesFile = Other.bGenerateDependenciesFile;
			bAllowRemotelyCompiledPCHs = Other.bAllowRemotelyCompiledPCHs;
			UserIncludePaths = new HashSet<DirectoryReference>(Other.UserIncludePaths);
			SystemIncludePaths = new HashSet<DirectoryReference>(Other.SystemIncludePaths);
			ModuleInterfacePaths = new HashSet<DirectoryReference>(Other.ModuleInterfacePaths);
			bCheckSystemHeadersForModification = Other.bCheckSystemHeadersForModification;
			ForceIncludeFiles.AddRange(Other.ForceIncludeFiles);
			AdditionalPrerequisites.AddRange(Other.AdditionalPrerequisites);
			Definitions.AddRange(Other.Definitions);
			AdditionalArguments = Other.AdditionalArguments;
			AdditionalFrameworks.AddRange(Other.AdditionalFrameworks);
			PrecompiledHeaderFile = Other.PrecompiledHeaderFile;
			bHackHeaderGenerator = Other.bHackHeaderGenerator;
			bHideSymbolsByDefault = Other.bHideSymbolsByDefault;
			CppStandard = Other.CppStandard;
			CStandard = Other.CStandard;
			bEnableCoroutines = Other.bEnableCoroutines;
			IncludeOrderVersion = Other.IncludeOrderVersion;
			bDeterministic = Other.bDeterministic;
			CrashDiagnosticDirectory = Other.CrashDiagnosticDirectory;
		}
	}
}
