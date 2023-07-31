// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Controls how a particular warning is treated
	/// </summary>
	public enum WarningLevel
	{
		/// <summary>
		/// Use the default behavior
		/// </summary>
		Default,

		/// <summary>
		/// Do not display diagnostics
		/// </summary>
		Off,

		/// <summary>
		/// Output warnings normally
		/// </summary>
		Warning,

		/// <summary>
		/// Output warnings as errors
		/// </summary>
		Error,
	}

	/// <summary>
	/// Describes the origin and visibility of Verse code
	/// </summary>
	public enum VerseScope
	{
		/// <summary>
		/// Created by Epic and is entirely hidden from public users
		/// </summary>
		InternalAPI,

		/// <summary>
		/// Created by Epic and only public definitions will be visible to public users
		/// </summary>
		PublicAPI,

		/// <summary>
		/// Created by a public user
		/// </summary>
		User
	}

	/// <summary>
	/// ModuleRules is a data structure that contains the rules for defining a module
	/// </summary>
	public class ModuleRules
	{
		/// <summary>
		/// Type of module
		/// </summary>
		public enum ModuleType
		{
			/// <summary>
			/// C++
			/// </summary>
			CPlusPlus,

			/// <summary>
			/// External (third-party)
			/// </summary>
			External,
		}

		/// <summary>
		/// Override the settings of the UHTModuleType to have a different set of
		/// PKG_ flags. Cannot set on a plugin because that value already set in
		/// the '.uplugin' file
		/// </summary>
		public enum PackageOverrideType
		{
			/// <summary>
			/// Do not override the package type on this module
			/// </summary>
			None,

			/// <summary>
			/// Set the PKG_EditorOnly flag on this module
			/// </summary>
			EditorOnly,

			/// <summary>
			/// Set the PKG_Developer on this module
			/// </summary>
			EngineDeveloper,

			/// <summary>
			/// Set the PKG_Developer on this module
			/// </summary>
			GameDeveloper,

			/// <summary>
			/// Set the PKG_UncookedOnly flag on this module
			/// </summary>
			EngineUncookedOnly,

			/// <summary>
			/// Set the PKG_UncookedOnly flag on this module as a game
			/// </summary>
			GameUncookedOnly
		}

		/// <summary>
		/// Code optimization settings
		/// </summary>
		public enum CodeOptimization
		{
			/// <summary>
			/// Code should never be optimized if possible.
			/// </summary>
			Never,

			/// <summary>
			/// Code should only be optimized in non-debug builds (not in Debug).
			/// </summary>
			InNonDebugBuilds,

			/// <summary>
			/// Code should only be optimized in shipping builds (not in Debug, DebugGame, Development)
			/// </summary>
			InShippingBuildsOnly,

			/// <summary>
			/// Code should always be optimized if possible.
			/// </summary>
			Always,

			/// <summary>
			/// Default: 'InNonDebugBuilds' for game modules, 'Always' otherwise.
			/// </summary>
			Default,
		}

		/// <summary>
		/// What type of PCH to use for this module.
		/// </summary>
		public enum PCHUsageMode
		{
			/// <summary>
			/// Default: Engine modules use shared PCHs, game modules do not
			/// </summary>
			Default,

			/// <summary>
			/// Never use any PCHs.
			/// </summary>
			NoPCHs,

			/// <summary>
			/// Never use shared PCHs.  Always generate a unique PCH for this module if appropriate
			/// </summary>
			NoSharedPCHs,

			/// <summary>
			/// Shared PCHs are OK!
			/// </summary>
			UseSharedPCHs,

			/// <summary>
			/// Shared PCHs may be used if an explicit private PCH is not set through PrivatePCHHeaderFile. In either case, none of the source files manually include a module PCH, and should include a matching header instead.
			/// </summary>
			UseExplicitOrSharedPCHs,
		}

		/// <summary>
		/// Which type of targets this module should be precompiled for
		/// </summary>
		public enum PrecompileTargetsType
		{
			/// <summary>
			/// Never precompile this module.
			/// </summary>
			None,

			/// <summary>
			/// Inferred from the module's directory. Engine modules under Engine/Source/Runtime will be compiled for games, those under Engine/Source/Editor will be compiled for the editor, etc...
			/// </summary>
			Default,

			/// <summary>
			/// Any game targets.
			/// </summary>
			Game,

			/// <summary>
			/// Any editor targets.
			/// </summary>
			Editor,

			/// <summary>
			/// Any targets.
			/// </summary>
			Any,
		}

		/// <summary>
		/// Control visibility of symbols in this module for special cases
		/// </summary>
		public enum SymbolVisibility
		{
			/// <summary>
			/// Standard visibility rules
			/// </summary>
			Default,

			/// <summary>
			/// Make sure symbols in this module are visible in Dll builds
			/// </summary>
			VisibileForDll,
		}

		/// <summary>
		/// Alter build order of source files for specific cases where necessary
		/// An example is test files that must be executed first for module level setup or last for module level teardown
		/// </summary>
		public enum SourceFileBuildOrder
		{
			/// <summary>
			/// Moves the order of the module source file at the beginning of compilation
			/// </summary>
			First,
			/// <summary>
			/// Moves the order of the module source file at the end of compilation
			/// </summary>
			Last,
		}

		/// <summary>
		/// Information about a file which is required by the target at runtime, and must be moved around with it.
		/// </summary>
		[Serializable]
		public class RuntimeDependency
		{
			/// <summary>
			/// The file that should be staged. Should use $(EngineDir) and $(ProjectDir) variables as a root, so that the target can be relocated to different machines.
			/// </summary>
			public string Path;

			/// <summary>
			/// The initial location for this file. It will be copied to Path at build time, ready for staging.
			/// </summary>
			public string? SourcePath;

			/// <summary>
			/// How to stage this file.
			/// </summary>
			public StagedFileType Type;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="InPath">Path to the runtime dependency</param>
			/// <param name="InType">How to stage the given path</param>
			public RuntimeDependency(string InPath, StagedFileType InType = StagedFileType.NonUFS)
			{
				Path = InPath;
				Type = InType;
			}

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="InPath">Path to the runtime dependency</param>
			/// <param name="InSourcePath">Source path for the file in the working tree</param>
			/// <param name="InType">How to stage the given path</param>
			public RuntimeDependency(string InPath, string InSourcePath, StagedFileType InType = StagedFileType.NonUFS)
			{
				Path = InPath;
				SourcePath = InSourcePath;
				Type = InType;
			}
		}

		/// <summary>
		/// List of runtime dependencies, with convenience methods for adding new items
		/// </summary>
		[Serializable]
		public class RuntimeDependencyList
		{
			/// <summary>
			/// Inner list of runtime dependencies
			/// </summary>
			internal List<RuntimeDependency> Inner = new List<RuntimeDependency>();

			/// <summary>
			/// Default constructor
			/// </summary>
			public RuntimeDependencyList()
			{
			}

			/// <summary>
			/// Add a runtime dependency to the list
			/// </summary>
			/// <param name="InPath">Path to the runtime dependency. May include wildcards.</param>
			public void Add(string InPath)
			{
				Inner.Add(new RuntimeDependency(InPath));
			}

			/// <summary>
			/// Add a runtime dependency to the list
			/// </summary>
			/// <param name="InPath">Path to the runtime dependency. May include wildcards.</param>
			/// <param name="InType">How to stage this file</param>
			public void Add(string InPath, StagedFileType InType)
			{
				Inner.Add(new RuntimeDependency(InPath, InType));
			}

			/// <summary>
			/// Add a runtime dependency to the list
			/// </summary>
			/// <param name="InPath">Path to the runtime dependency. May include wildcards.</param>
			/// <param name="InSourcePath">Source path for the file to be added as a dependency. May include wildcards.</param>
			/// <param name="InType">How to stage this file</param>
			public void Add(string InPath, string InSourcePath, StagedFileType InType = StagedFileType.NonUFS)
			{
				Inner.Add(new RuntimeDependency(InPath, InSourcePath, InType));
			}
		}

		/// <summary>
		/// List of runtime dependencies, with convenience methods for adding new items
		/// </summary>
		[Serializable]
		public class ReceiptPropertyList
		{
			/// <summary>
			/// Inner list of runtime dependencies
			/// </summary>
			internal List<ReceiptProperty> Inner = new List<ReceiptProperty>();

			/// <summary>
			/// Default constructor
			/// </summary>
			public ReceiptPropertyList()
			{
			}

			/// <summary>
			/// Add a receipt property to the list
			/// </summary>
			/// <param name="Name">Name of the property</param>
			/// <param name="Value">Value for the property</param>
			public void Add(string Name, string Value)
			{
				Inner.Add(new ReceiptProperty(Name, Value));
			}
		}

		/// <summary>
		/// Stores information about a framework on IOS or MacOS
		/// </summary>
		public class Framework
		{
			/// <summary>
			/// Name of the framework
			/// </summary>
			internal string Name;

			/// <summary>
			/// Specifies the path to a zip file that contains it or where the framework is located on disk
			/// </summary>
			internal string Path;

			/// <summary>
			/// 
			/// </summary>
			internal string? CopyBundledAssets = null;

			/// <summary>
			/// Copy the framework to the target's Framework directory
			/// </summary>
			internal bool bCopyFramework = false;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="Name">Name of the framework</param>
			/// <param name="Path">Path to a zip file containing the framework or a framework on disk</param>
			/// <param name="CopyBundledAssets"></param>
			/// <param name="bCopyFramework">Copy the framework to the target's Framework directory</param>
			public Framework(string Name, string Path, string? CopyBundledAssets = null, bool bCopyFramework = false)
			{
				this.Name = Name;
				this.Path = Path;
				this.CopyBundledAssets = CopyBundledAssets;
				this.bCopyFramework = bCopyFramework;
			}

			/// <summary>
			/// Specifies if the file is a zip file
			/// </summary>
			public bool IsZipFile()
			{
				return Path.EndsWith(".zip");
			}
		}

		/// <summary>
		/// 
		/// </summary>
		public class BundleResource
		{
			/// <summary>
			/// 
			/// </summary>
			public string? ResourcePath = null;

			/// <summary>
			/// 
			/// </summary>
			public string? BundleContentsSubdir = null;

			/// <summary>
			/// 
			/// </summary>
			public bool bShouldLog = true;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="ResourcePath"></param>
			/// <param name="BundleContentsSubdir"></param>
			/// <param name="bShouldLog"></param>
			public BundleResource(string ResourcePath, string BundleContentsSubdir = "Resources", bool bShouldLog = true)
			{
				this.ResourcePath = ResourcePath;
				this.BundleContentsSubdir = BundleContentsSubdir;
				this.bShouldLog = bShouldLog;
			}
		}

		/// <summary>
		/// Information about a Windows type library (TLB/OLB file) which requires a generated header.
		/// </summary>
		public class TypeLibrary
		{
			/// <summary>
			/// Name of the type library
			/// </summary>
			public string FileName;

			/// <summary>
			/// Additional attributes for the #import directive
			/// </summary>
			public string Attributes;

			/// <summary>
			/// Name of the output header
			/// </summary>
			public string Header;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="FileName">Name of the type library. Follows the same conventions as the filename parameter in the MSVC #import directive.</param>
			/// <param name="Attributes">Additional attributes for the import directive</param>
			/// <param name="Header">Name of the output header</param>
			public TypeLibrary(string FileName, string Attributes, string Header)
			{
				this.FileName = FileName;
				this.Attributes = Attributes;
				this.Header = Header;
			}
		}

		/// <summary>
		/// Specifies build order overrides for source files in this module
		/// A source file can be moved towards the beginning or end of compilation
		/// Example use case: test setup and test teardown files that need to compile first and last respectively in a module
		/// </summary>
		public class SourceFilesBuildOrderSettings
		{
			private DirectoryReference ModuleDirectory;
			private Dictionary<FileItem, SourceFileBuildOrder> BuildOrderOverridesPrivate;

			/// <summary>
			/// Constructs <see cref="SourceFilesBuildOrderSettings"/> given module directory.
			/// </summary>
			/// <param name="InModuleDirectory">Module source directory</param>
			public SourceFilesBuildOrderSettings(DirectoryReference InModuleDirectory)
			{
				ModuleDirectory = InModuleDirectory;
				BuildOrderOverridesPrivate = new Dictionary<FileItem, SourceFileBuildOrder>();
			}

			/// <summary>
			/// Slightly alter the order of build of a module source file by placing it either at the beginning or end of compilation
			/// </summary>
			/// <param name="InRelativeSourceFile">Relative path of source file to module's directory</param>
			/// <param name="InBuildOrderOverride">A <see cref="SourceFileBuildOrder"/> specifying order placement: 
			/// <see cref="SourceFileBuildOrder.First"/> for beginning of compilation, <see cref="SourceFileBuildOrder.Last"/> for end</param>
			public void AddBuildOrderOverride(string InRelativeSourceFile, SourceFileBuildOrder InBuildOrderOverride)
			{
				FileItem File = FileItem.GetItemByPath(Path.Combine(ModuleDirectory.FullName, InRelativeSourceFile));
				if (File.Exists)
				{
					BuildOrderOverridesPrivate.Add(File, InBuildOrderOverride);
				}
				else
				{
					throw new BuildException($"Cannot apply build order override, file doesn't exist: {File.AbsolutePath}");
				}
			}

			/// <summary>
			/// Get build order overrides map of module source file to <see cref="SourceFileBuildOrder"/>.
			/// </summary>
			public IReadOnlyDictionary<FileItem, SourceFileBuildOrder> Overrides => BuildOrderOverridesPrivate;
		}

		/// <summary>
		/// Name of this module
		/// </summary>
		public string Name
		{
			get;
			internal set;
		}

		/// <summary>
		/// File containing this module
		/// </summary>
		internal FileReference File { get; set; }

		/// <summary>
		/// Directory containing this module
		/// </summary>
		internal DirectoryReference Directory { get; set; }

		/// <summary>
		/// Additional directories that contribute to this module (likely in UnrealBuildTool.EnginePlatformExtensionsDirectory). 
		/// The dictionary tracks module subclasses 
		/// </summary>
		internal Dictionary<Type, DirectoryReference>? DirectoriesForModuleSubClasses;

		/// <summary>
		/// Additional directories that contribute to this module but are not based on a subclass (NotForLicensees, etc)
		/// </summary>
		private List<DirectoryReference> AdditionalModuleDirectories = new List<DirectoryReference>();

		/// <summary>
		/// The rules assembly to use when searching for modules
		/// </summary>
		internal RulesAssembly RulesAssembly;

		/// <summary>
		/// Plugin containing this module
		/// </summary>
		internal PluginInfo? Plugin;

		/// <summary>
		/// True if a Plugin contains this module
		/// </summary>
		public bool IsPlugin
		{
			get
			{
				return Plugin != null;
			}
		}

		/// <summary>
		/// The rules context for this instance
		/// </summary>
		internal ModuleRulesContext Context { get; set; }

		/// <summary>
		/// Rules for the target that this module belongs to
		/// </summary>
		public readonly ReadOnlyTargetRules Target;

		/// <summary>
		/// Type of module
		/// </summary>
		public ModuleType Type = ModuleType.CPlusPlus;

		/// <summary>
		/// Overridden type of module that will set different package flags.
		/// Cannot be used for modules that are a part of a plugin because that is 
		/// set in the `.uplugin` file already. 
		/// </summary>
		public PackageOverrideType OverridePackageType
		{
			get { return overridePackageType ?? PackageOverrideType.None; }
			set
			{
				if (!IsPlugin)
				{
					overridePackageType = value;
				}
				else
				{
					throw new BuildException("Module '{0}' cannot override package type because it is part of a plugin!", Name);
				}
			}
		}

		private PackageOverrideType? overridePackageType;

		/// <summary>
		/// Returns true if there has been an override type specified on this module
		/// </summary>
		public bool HasPackageOverride
		{
			get
			{
				return OverridePackageType != PackageOverrideType.None;
			}
		}

		/// <summary>
		/// Subfolder of Binaries/PLATFORM folder to put this module in when building DLLs. This should only be used by modules that are found via searching like the
		/// TargetPlatform or ShaderFormat modules. If FindModules is not used to track them down, the modules will not be found.
		/// </summary>
		public string BinariesSubFolder = "";

		private CodeOptimization? OptimizeCodeOverride;

		/// <summary>
		/// When this module's code should be optimized.
		/// </summary>
		public CodeOptimization OptimizeCode
		{
			get
			{
				if (OptimizeCodeOverride.HasValue)
					return OptimizeCodeOverride.Value;

				bool? ShouldOptimizeCode = null;
				if (Target.EnableOptimizeCodeForModules?.Contains(Name) ?? false)
					ShouldOptimizeCode = true;
				if (Target.DisableOptimizeCodeForModules?.Contains(Name) ?? false)
					ShouldOptimizeCode = false;

				if (!ShouldOptimizeCode.HasValue)
					return CodeOptimization.Default;

				return ShouldOptimizeCode.Value ? CodeOptimization.Always : CodeOptimization.Never;
			}
			set { OptimizeCodeOverride = value; }
		}

		/// <summary>
		/// Explicit private PCH for this module. Implies that this module will not use a shared PCH.
		/// </summary>
		public string? PrivatePCHHeaderFile;

		/// <summary>
		/// Header file name for a shared PCH provided by this module.  Must be a valid relative path to a public C++ header file.
		/// This should only be set for header files that are included by a significant number of other C++ modules.
		/// </summary>
		public string? SharedPCHHeaderFile;

		/// <summary>
		/// Specifies an alternate name for intermediate directories and files for intermediates of this module. Useful when hitting path length limitations.
		/// </summary>
		public string? ShortName = null;

		/// <summary>
		/// Precompiled header usage for this module
		/// </summary>
		public PCHUsageMode PCHUsage
		{
			get
			{
				if (PCHUsagePrivate.HasValue)
				{
					// Use the override
					return PCHUsagePrivate.Value;
				}
				else if (Target.bEnableCppModules)
				{
					return PCHUsageMode.NoPCHs;
				}
				else if (Target.bIWYU || DefaultBuildSettings >= BuildSettingsVersion.V2)
				{
					// Use shared or explicit PCHs, and enable IWYU
					return PCHUsageMode.UseExplicitOrSharedPCHs;
				}
				else if (Plugin != null)
				{
					// Older plugins use shared PCHs by default, since they aren't typically large enough to warrant their own PCH.
					return PCHUsageMode.UseSharedPCHs;
				}
				else
				{
					// Older game modules do not enable shared PCHs by default, because games usually have a large precompiled header of their own.
					return PCHUsageMode.NoSharedPCHs;
				}
			}
			set { PCHUsagePrivate = value; }
		}
		private PCHUsageMode? PCHUsagePrivate;

		/// <summary>
		/// Whether this module should be treated as an engine module (eg. using engine definitions, PCHs, compiled with optimizations enabled in DebugGame configurations, etc...).
		/// Initialized to a default based on the rules assembly it was created from.
		/// </summary>
		public bool bTreatAsEngineModule;

		/// <summary>
		/// Which engine version's build settings to use by default. 
		/// </summary>
		public BuildSettingsVersion DefaultBuildSettings
		{
			get { return DefaultBuildSettingsPrivate ?? Target.DefaultBuildSettings; }
			set { DefaultBuildSettingsPrivate = value; }
		}
		private BuildSettingsVersion? DefaultBuildSettingsPrivate;

		/// <summary>
		/// What version of include order to use when compiling this module. Can be overridden via -ForceIncludeOrder on the command line or in a module's rules.
		/// </summary>
		public EngineIncludeOrderVersion IncludeOrderVersion
		{
			get
			{
				if (Target.ForcedIncludeOrder != null)
				{
					return Target.ForcedIncludeOrder.Value;
				}
				if (bTreatAsEngineModule)
				{
					return EngineIncludeOrderVersion.Latest;
				}
				return IncludeOrderVersionPrivate ?? Target.IncludeOrderVersion;
			}
			set { IncludeOrderVersionPrivate = value; }
		}
		private EngineIncludeOrderVersion? IncludeOrderVersionPrivate;

		/// <summary>
		/// Set flags for determinstic compiles (experimental, may not be fully supported). Deterministic linking is controlled via TargetRules.
		/// </summary>
		public bool bDeterministic = false;

		/// <summary>
		/// Use run time type information
		/// </summary>
		public bool bUseRTTI = false;

		/// <summary>
		/// Direct the compiler to generate AVX instructions wherever SSE or AVX intrinsics are used, on the platforms that support it.
		/// Note that by enabling this you are changing the minspec for the PC platform, and the resultant executable will crash on machines without AVX support.
		/// </summary>
		public bool bUseAVX = false;

		/// <summary>
		/// Enable buffer security checks.  This should usually be enabled as it prevents severe security risks.
		/// </summary>
		public bool bEnableBufferSecurityChecks = true;

		/// <summary>
		/// Enable exception handling
		/// </summary>
		public bool bEnableExceptions = false;

		/// <summary>
		/// Enable objective C exception handling
		/// </summary>
		public bool bEnableObjCExceptions = false;

		/// <summary>
		/// How to treat shadow variable warnings
		/// </summary>
		public WarningLevel ShadowVariableWarningLevel
		{
			get { return (ShadowVariableWarningLevelPrivate == WarningLevel.Default)? ((DefaultBuildSettings >= BuildSettingsVersion.V2) ? WarningLevel.Error : Target.ShadowVariableWarningLevel) : ShadowVariableWarningLevelPrivate; }
			set { ShadowVariableWarningLevelPrivate = value; }
		}

		/// <inheritdoc cref="ShadowVariableWarningLevelPrivate"/>
		private WarningLevel ShadowVariableWarningLevelPrivate;

		/// <summary>
		/// How to treat unsafe implicit type cast warnings (e.g., double->float or int64->int32)
		/// </summary>
		public WarningLevel UnsafeTypeCastWarningLevel
		{
			get { return (UnsafeTypeCastWarningLevelPrivate == WarningLevel.Default)? Target.UnsafeTypeCastWarningLevel : UnsafeTypeCastWarningLevelPrivate; }
			set { UnsafeTypeCastWarningLevelPrivate = value; }
		}

		/// <inheritdoc cref="UnsafeTypeCastWarningLevel"/>
		private WarningLevel UnsafeTypeCastWarningLevelPrivate;

		/// <summary>
		/// Enable warnings for using undefined identifiers in #if expressions
		/// </summary>
		public bool bEnableUndefinedIdentifierWarnings = true;

		/// <summary>
		/// Disable all static analysis - clang, msvc, pvs-studio.
		/// </summary>
		public bool bDisableStaticAnalysis = false;

		/// <summary>
		/// The static analyzer checkers that should be enabled rather than the defaults. This is only supported for Clang.
		/// See https://clang.llvm.org/docs/analyzer/checkers.html for a full list. Or run:
		///    'clang -Xclang -analyzer-checker-help' 
		/// or: 
		///    'clang -Xclang -analyzer-checker-help-alpha' 
		/// for the list of experimental checkers.
		/// </summary>
		public HashSet<string> StaticAnalyzerCheckers = new HashSet<string>();

		/// <summary>
		/// The static analyzer default checkers that should be disabled. Unused if StaticAnalyzerCheckers is populated. This is only supported for Clang.
		/// This overrides the default disabled checkers, which are deadcode.DeadStores and security.FloatLoopCounter
		/// See https://clang.llvm.org/docs/analyzer/checkers.html for a full list. Or run:
		///    'clang -Xclang -analyzer-checker-help' 
		/// or: 
		///    'clang -Xclang -analyzer-checker-help-alpha' 
		/// for the list of experimental checkers.
		/// </summary>
		public HashSet<string> StaticAnalyzerDisabledCheckers = new HashSet<string>() { "deadcode.DeadStores", "security.FloatLoopCounter" };

		/// <summary>
		/// The static analyzer non-default checkers that should be enabled. Unused if StaticAnalyzerCheckers is populated. This is only supported for Clang.
		/// See https://clang.llvm.org/docs/analyzer/checkers.html for a full list. Or run:
		///    'clang -Xclang -analyzer-checker-help' 
		/// or: 
		///    'clang -Xclang -analyzer-checker-help-alpha' 
		/// for the list of experimental checkers.
		/// </summary>
		public HashSet<string> StaticAnalyzerAdditionalCheckers = new HashSet<string>();

		private bool? bUseUnityOverride;
		/// <summary>
		/// If unity builds are enabled this can be used to override if this specific module will build using Unity.
		/// This is set using the per module configurations in BuildConfiguration.
		/// </summary>
		public bool bUseUnity
		{
			set { bUseUnityOverride = value; }
			get
			{
				bool UseUnity = true;
				if (Target.DisableUnityBuildForModules?.Contains(Name) ?? false)
					UseUnity = false;
				return bUseUnityOverride ?? UseUnity;
			}
		}

		/// <summary>
		/// Whether to merge module and generated unity files for faster compilation.
		/// </summary>
		public bool bMergeUnityFiles = true;

		/// <summary>
		/// The number of source files in this module before unity build will be activated for that module.  If set to
		/// anything besides -1, will override the default setting which is controlled by MinGameModuleSourceFilesForUnityBuild
		/// </summary>
		public int MinSourceFilesForUnityBuildOverride = 0;

		/// <summary>
		/// Overrides BuildConfiguration.MinFilesUsingPrecompiledHeader if non-zero.
		/// </summary>
		public int MinFilesUsingPrecompiledHeaderOverride = 0;

		/// <summary>
		/// Overrides Target.NumIncludedBytesPerUnityCPP if non-zero.
		/// </summary>
		public int NumIncludedBytesPerUnityCPPOverride = 0;

		/// <summary>
		/// Module uses a #import so must be built locally when compiling with SN-DBS
		/// </summary>
		public bool bBuildLocallyWithSNDBS = false;

		/// <summary>
		/// Enable warnings for when there are .gen.cpp files that could be inlined in a matching handwritten cpp file
		/// </summary>
		public bool bEnableNonInlinedGenCppWarnings = false;

		/// <summary>
		/// Redistribution override flag for this module.
		/// </summary>
		public bool? IsRedistributableOverride = null;

		/// <summary>
		/// Whether the output from this module can be publicly distributed, even if it has code/
		/// dependencies on modules that are not (i.e. CarefullyRedist, NotForLicensees, NoRedist).
		/// This should be used when you plan to release binaries but not source.
		/// </summary>
		public bool bLegalToDistributeObjectCode = false;

		/// <summary>
		/// List of folders which are allowed to be referenced when compiling this binary, without propagating restricted folder names
		/// </summary>
		public List<string> AllowedRestrictedFolders = new List<string>();

		/// <summary>
		/// Set of aliased restricted folder references
		/// </summary>
		public Dictionary<string, string> AliasRestrictedFolders = new Dictionary<string, string>();

		/// <summary>
		/// Enforce "include what you use" rules when PCHUsage is set to ExplicitOrSharedPCH; warns when monolithic headers (Engine.h, UnrealEd.h, etc...) 
		/// are used, and checks that source files include their matching header first.
		/// </summary>
		public bool bEnforceIWYU = true;

		/// <summary>
		/// Whether to add all the default include paths to the module (eg. the Source/Classes folder, subfolders under Source/Public).
		/// </summary>
		public bool bAddDefaultIncludePaths = true;

		/// <summary>
		/// Whether to ignore dangling (i.e. unresolved external) symbols in modules
		/// </summary>
		public bool bIgnoreUnresolvedSymbols = false;

		/// <summary>
		/// Whether this module should be precompiled. Defaults to the bPrecompile flag from the target. Clear this flag to prevent a module being precompiled.
		/// </summary>
		public bool bPrecompile;

		/// <summary>
		/// Whether this module should use precompiled data. Always true for modules created from installed assemblies.
		/// </summary>
		public bool bUsePrecompiled;

		/// <summary>
		/// Whether this module can use PLATFORM_XXXX style defines, where XXXX is a confidential platform name. This is used to ensure engine or other 
		/// shared code does not reveal confidential information inside an #if PLATFORM_XXXX block. Licensee game code may want to allow for them, however.
		/// </summary>
		public bool bAllowConfidentialPlatformDefines = false;

		/// <summary>
		/// List of modules names (no path needed) with header files that our module's public headers needs access to, but we don't need to "import" or link against.
		/// </summary>
		public List<string> PublicIncludePathModuleNames = new List<string>();

		/// <summary>
		/// List of public dependency module names (no path needed) (automatically does the private/public include). These are modules that are required by our public source files.
		/// </summary>
		public List<string> PublicDependencyModuleNames = new List<string>();

		/// <summary>
		/// List of modules name (no path needed) with header files that our module's private code files needs access to, but we don't need to "import" or link against.
		/// </summary>
		public List<string> PrivateIncludePathModuleNames = new List<string>();

		/// <summary>
		/// List of private dependency module names.  These are modules that our private code depends on but nothing in our public
		/// include files depend on.
		/// </summary>
		public List<string> PrivateDependencyModuleNames = new List<string>();

		/// <summary>
		/// Only for legacy reason, should not be used in new code. List of module dependencies that should be treated as circular references.  This modules must have already been added to
		/// either the public or private dependent module list.
		/// </summary>
		public List<string> CircularlyReferencedDependentModules = new List<string>();

		/// <summary>
		/// List of system/library include paths - typically used for External (third party) modules.  These are public stable header file directories that are not checked when resolving header dependencies.
		/// </summary>
		public List<string> PublicSystemIncludePaths = new List<string>();

		/// <summary>
		/// (This setting is currently not need as we discover all files from the 'Public' folder) List of all paths to include files that are exposed to other modules
		/// </summary>
		public List<string> PublicIncludePaths = new List<string>();

		/// <summary>
		/// (This setting is currently not need as we discover all files from the 'Internal' folder) List of all paths to include files that are exposed to other internal modules
		/// </summary>
		public List<string> InternalncludePaths = new List<string>();

		/// <summary>
		/// List of all paths to this module's internal include files, not exposed to other modules (at least one include to the 'Private' path, more if we want to avoid relative paths)
		/// </summary>
		public List<string> PrivateIncludePaths = new List<string>();

		/// <summary>
		/// List of system library paths (directory of .lib files) - for External (third party) modules please use the PublicAdditionalLibaries instead
		/// </summary>
		public List<string> PublicSystemLibraryPaths = new List<string>();

		/// <summary>
		/// List of search paths for libraries at runtime (eg. .so files)
		/// </summary>
		public List<string> PrivateRuntimeLibraryPaths = new List<string>();

		/// <summary>
		/// List of search paths for libraries at runtime (eg. .so files)
		/// </summary>
		public List<string> PublicRuntimeLibraryPaths = new List<string>();

		/// <summary>
		/// List of additional libraries (names of the .lib files including extension) - typically used for External (third party) modules
		/// </summary>
		public List<string> PublicAdditionalLibraries = new List<string>();

		/// <summary>
		/// Returns the directory of where the passed in module name lives.
		/// </summary>
		/// <param name="ModuleName">Name of the module</param>
		/// <returns>Directory where the module lives</returns>
		public string GetModuleDirectory(string ModuleName)
		{
			FileReference? ModuleFileReference = RulesAssembly.GetModuleFileName(ModuleName);
			if (ModuleFileReference == null)
			{
				throw new BuildException("Could not find a module named '{0}'.", ModuleName);
			}
			return ModuleFileReference.Directory.FullName;
		}

		/// <summary>
		/// List of additional pre-build libraries (names of the .lib files including extension) - typically used for additional targets which are still built, but using either TargetRules.PreBuildSteps or TargetRules.PreBuildTargets.
		/// </summary>
		public List<string> PublicPreBuildLibraries = new List<string>();

		/// <summary>
		/// List of system libraries to use - these are typically referenced via name and then found via the system paths. If you need to reference a .lib file use the PublicAdditionalLibraries instead
		/// </summary>
		public List<string> PublicSystemLibraries = new List<string>();

		/// <summary>
		/// List of XCode frameworks (iOS and MacOS)
		/// </summary>
		public List<string> PublicFrameworks = new List<string>();

		/// <summary>
		/// List of weak frameworks (for OS version transitions)
		/// </summary>
		public List<string> PublicWeakFrameworks = new List<string>();

		/// <summary>
		/// List of addition frameworks - typically used for External (third party) modules on Mac and iOS
		/// </summary>
		public List<Framework> PublicAdditionalFrameworks = new List<Framework>();

		/// <summary>
		/// List of addition resources that should be copied to the app bundle for Mac or iOS
		/// </summary>
		public List<BundleResource> AdditionalBundleResources = new List<BundleResource>();

		/// <summary>
		/// List of type libraries that we need to generate headers for (Windows only)
		/// </summary>
		public List<TypeLibrary> TypeLibraries = new List<TypeLibrary>();

		/// <summary>
		/// List of delay load DLLs - typically used for External (third party) modules
		/// </summary>
		public List<string> PublicDelayLoadDLLs = new List<string>();

		/// <summary>
		/// Private compiler definitions for this module
		/// </summary>
		public List<string> PrivateDefinitions = new List<string>();

		/// <summary>
		/// Public compiler definitions for this module
		/// </summary>
		public List<string> PublicDefinitions = new List<string>();

		/// <summary>
		/// Append (or create)
		/// </summary>
		/// <param name="Definition"></param>
		/// <param name="Text"></param>
		public void AppendStringToPublicDefinition(string Definition, string Text)
		{
			string WithEquals = Definition + "=";
			for (int Index = 0; Index < PublicDefinitions.Count; Index++)
			{
				if (PublicDefinitions[Index].StartsWith(WithEquals))
				{
					PublicDefinitions[Index] = PublicDefinitions[Index] + Text;
					return;
				}
			}

			// if we get here, we need to make a new entry
			PublicDefinitions.Add(Definition + "=" + Text);
		}

		/// <summary>
		/// Addition modules this module may require at run-time 
		/// </summary>
		public List<string> DynamicallyLoadedModuleNames = new List<string>();

		/// <summary>
		/// List of files which this module depends on at runtime. These files will be staged along with the target.
		/// </summary>
		public RuntimeDependencyList RuntimeDependencies = new RuntimeDependencyList();

		/// <summary>
		/// List of additional properties to be added to the build receipt
		/// </summary>
		public ReceiptPropertyList AdditionalPropertiesForReceipt = new ReceiptPropertyList();

		/// <summary>
		/// Which targets this module should be precompiled for
		/// </summary>
		public PrecompileTargetsType PrecompileForTargets = PrecompileTargetsType.Default;

		/// <summary>
		/// External files which invalidate the makefile if modified. Relative paths are resolved relative to the .build.cs file.
		/// </summary>
		public List<string> ExternalDependencies = new List<string>();

		/// <summary>
		/// Subclass rules files which invalidate the makefile if modified.
		/// </summary>
		public List<string>? SubclassRules;

		/// <summary>
		/// Whether this module requires the IMPLEMENT_MODULE macro to be implemented. Most UE modules require this, since we use the IMPLEMENT_MODULE macro
		/// to do other global overloads (eg. operator new/delete forwarding to GMalloc).
		/// </summary>
		public bool? bRequiresImplementModule;

		/// <summary>
		/// If this module has associated Verse code, this is the Verse root path of it
		/// </summary>
		public string? VersePath;

		/// <summary>
		/// Visibility of Verse code in this module's Source/Verse folder
		/// </summary>
		public VerseScope VerseScope = VerseScope.User;

		/// <summary>
		/// Whether this module qualifies included headers from other modules relative to the root of their 'Public' folder. This reduces the number
		/// of search paths that have to be passed to the compiler, improving performance and reducing the length of the compiler command line.
		/// </summary>
		public bool bLegacyPublicIncludePaths
		{
			set { bLegacyPublicIncludePathsPrivate = value; }
			get { return bLegacyPublicIncludePathsPrivate ?? ((DefaultBuildSettings < BuildSettingsVersion.V2) ? Target.bLegacyPublicIncludePaths : false); }
		}
		private bool? bLegacyPublicIncludePathsPrivate;
		
		/// <summary>
		/// Whether circular dependencies will be validated against the allow list
		/// Circular module dependencies result in slower builds. Disabling this option is strongly discouraged.
        /// This option is ignored for Engine modules which will always be validated against the allow list.
		/// </summary>
		public bool bValidateCircularDependencies = true;

		/// <summary>
		/// Which stanard to use for compiling this module
		/// </summary>
		public CppStandardVersion CppStandard = CppStandardVersion.Default;
		
		/// <summary>
		/// Which standard to use for compiling this module
		/// </summary>
		public CStandardVersion CStandard = CStandardVersion.Default;


		/// <summary>
		///  Control visibility of symbols
		/// </summary>
		public SymbolVisibility ModuleSymbolVisibility = ModuleRules.SymbolVisibility.Default;

		/// <summary>
		/// The AutoSDK directory for the active host platform
		/// </summary>
		public string? AutoSdkDirectory
		{
			get
			{
				DirectoryReference? AutoSdkDir;
				return UEBuildPlatformSDK.TryGetHostPlatformAutoSDKDir(out AutoSdkDir) ? AutoSdkDir.FullName : null;
			}
		}

		/// <summary>
		/// The current engine directory
		/// </summary>
		public string EngineDirectory
		{
			get
			{
				return Unreal.EngineDirectory.FullName;
			}
		}

		/// <summary>
		/// Property for the directory containing this plugin. Useful for adding paths to third party dependencies.
		/// </summary>
		public string PluginDirectory
		{
			get
			{
				if (Plugin == null)
				{
					throw new BuildException("Module '{0}' does not belong to a plugin; PluginDirectory property is invalid.", Name);
				}
				else
				{
					return Plugin.Directory.FullName;
				}
			}
		}

		/// <summary>
		/// Property for the directory containing this module. Useful for adding paths to third party dependencies.
		/// </summary>
		public string ModuleDirectory
		{
			get
			{
				return Directory.FullName;
			}
		}

		/// <summary>
		/// Returns module's low level tests directory "Tests".
		/// </summary>
		public string TestsDirectory
		{
			get
			{
				return Path.Combine(Directory.FullName, "Tests");
			}
		}

		/// <summary>
		/// Optional compilation order override rules for module's source files.
		/// </summary>
		public SourceFilesBuildOrderSettings BuildOrderSettings
		{
			get
			{
				if (BuildOrderOverridesPrivate == null)
				{
					BuildOrderOverridesPrivate = new SourceFilesBuildOrderSettings(Directory);
				}
				return BuildOrderOverridesPrivate;
			}
		}
		private SourceFilesBuildOrderSettings BuildOrderOverridesPrivate;

#nullable disable
		/// <summary>
		/// Constructor. For backwards compatibility while the parameterless constructor is being phased out, initialization which would happen here is done by 
		/// RulesAssembly.CreateModulRules instead.
		/// </summary>
		/// <param name="Target">Rules for building this target</param>
		public ModuleRules(ReadOnlyTargetRules Target)
		{
			this.Target = Target;
		}
#nullable restore

		/// <summary>
		/// Add the given Engine ThirdParty modules as static private dependencies
		///	Statically linked to this module, meaning they utilize exports from the other module
		///	Private, meaning the include paths for the included modules will not be exposed when giving this modules include paths
		///	NOTE: There is no AddThirdPartyPublicStaticDependencies function.
		/// </summary>
		/// <param name="Target">The target this module belongs to</param>
		/// <param name="ModuleNames">The names of the modules to add</param>
		public void AddEngineThirdPartyPrivateStaticDependencies(ReadOnlyTargetRules Target, params string[] ModuleNames)
		{
			if (!bUsePrecompiled || Target.LinkType == TargetLinkType.Monolithic)
			{
				PrivateDependencyModuleNames.AddRange(ModuleNames);
			}
		}

		/// <summary>
		/// Add the given Engine ThirdParty modules as dynamic private dependencies
		///	Dynamically linked to this module, meaning they do not utilize exports from the other module
		///	Private, meaning the include paths for the included modules will not be exposed when giving this modules include paths
		///	NOTE: There is no AddThirdPartyPublicDynamicDependencies function.
		/// </summary>
		/// <param name="Target">Rules for the target being built</param>
		/// <param name="ModuleNames">The names of the modules to add</param>
		public void AddEngineThirdPartyPrivateDynamicDependencies(ReadOnlyTargetRules Target, params string[] ModuleNames)
		{
			if (!bUsePrecompiled || Target.LinkType == TargetLinkType.Monolithic)
			{
				PrivateIncludePathModuleNames.AddRange(ModuleNames);
				DynamicallyLoadedModuleNames.AddRange(ModuleNames);
			}
		}

		/// <summary>
		/// Setup this module for Mesh Editor support (based on the settings in UEBuildConfiguration)
		/// </summary>
		public void EnableMeshEditorSupport(ReadOnlyTargetRules Target)
		{
			if (Target.bEnableMeshEditor == true)
			{
				PublicDefinitions.Add("ENABLE_MESH_EDITOR=1");
			}
			else
			{
				PublicDefinitions.Add("ENABLE_MESH_EDITOR=0");
			}
		}

		/// <summary>
		/// Setup this module for GameplayDebugger support
		/// </summary>
		public void SetupGameplayDebuggerSupport(ReadOnlyTargetRules Target, bool bAddAsPublicDependency = false)
		{
			if (Target.bUseGameplayDebugger)
			{
				PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=1");

				if (bAddAsPublicDependency)
				{
					PublicDependencyModuleNames.Add("GameplayDebugger");
				}
				else
				{
					PrivateDependencyModuleNames.Add("GameplayDebugger");
				}
			}
			else
			{
				PublicDefinitions.Add("WITH_GAMEPLAY_DEBUGGER=0");
			}
		}

		/// <summary>
		/// Setup this module for Iris support (based on the settings in UEBuildConfiguration)
		/// </summary>
		public void SetupIrisSupport(ReadOnlyTargetRules Target, bool bAddAsPublicDependency = false)
		{
			if (Target.bUseIris == true)
			{
				PublicDefinitions.Add("UE_WITH_IRIS=1");

				if (bAddAsPublicDependency)
				{
					PublicDependencyModuleNames.Add("IrisCore");
				}
				else
				{
					PrivateDependencyModuleNames.Add("IrisCore");
				}
			}
			else
			{
				PublicDefinitions.Add("UE_WITH_IRIS=0");

				// If we compile without Iris we have a stub Iris module for UHT dependencies
				if (bAddAsPublicDependency)
				{
					PublicDependencyModuleNames.Add("IrisStub");
				}
				else
				{
					PrivateDependencyModuleNames.Add("IrisStub");
				}
			}
		}

		/// <summary>
		/// Setup this module for physics support (based on the settings in UEBuildConfiguration)
		/// </summary>
		public void SetupModulePhysicsSupport(ReadOnlyTargetRules Target)
		{
			PublicIncludePathModuleNames.AddRange(
					new string[] {
					"Chaos",
					}
				);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"PhysicsCore",
					"Chaos",
					}
				);

			PublicDefinitions.Add("WITH_CLOTH_COLLISION_DETECTION=1");

			// Modules may still be relying on appropriate definitions for physics.
			// Nothing in engine should use these anymore as they were all deprecated and 
			// assumed to be in the following configuration from 5.1, this will cause
			// deprecation warning to fire in any module still relying on these macros

			Func<string, string, string, string> GetDeprecatedPhysicsMacro = (string Macro, string Value, string Version) =>
			{
				return Macro + "=UE_DEPRECATED_MACRO(" + Version + ", \"" + Macro + " is deprecated and should always be considered " + Value + ".\") " + Value;
			};

			PublicDefinitions.AddRange(
				new string[]{
					GetDeprecatedPhysicsMacro("INCLUDE_CHAOS", "1", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_CHAOS", "1", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_CHAOS_CLOTHING", "1", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_CHAOS_NEEDS_TO_BE_FIXED", "1", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_PHYSX", "1", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_PHYSX_COOKING", "0", "5.1"),
					GetDeprecatedPhysicsMacro("PHYSICS_INTERFACE_PHYSX", "0", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_APEX", "0", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_APEX_CLOTHING", "0", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_NVCLOTH", "0", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_IMMEDIATE_PHYSX", "0", "5.1"),
					GetDeprecatedPhysicsMacro("WITH_CUSTOM_SQ_STRUCTURE", "0", "5.1")
				});
		}

		/// <summary>
		/// Determines if this module can be precompiled for the current target.
		/// </summary>
		/// <param name="RulesFile">Path to the module rules file</param>
		/// <returns>True if the module can be precompiled, false otherwise</returns>
		internal bool IsValidForTarget(FileReference RulesFile)
		{
			if(Type == ModuleRules.ModuleType.CPlusPlus)
			{
				switch (PrecompileForTargets)
				{
					case ModuleRules.PrecompileTargetsType.None:
						return false;
					case ModuleRules.PrecompileTargetsType.Default:
						return (Target.Type == TargetType.Editor || !Unreal.GetExtensionDirs(Unreal.EngineDirectory, "Source/Developer").Any(Dir => RulesFile.IsUnderDirectory(Dir)) || Plugin != null);
					case ModuleRules.PrecompileTargetsType.Game:
						return (Target.Type == TargetType.Client || Target.Type == TargetType.Server || Target.Type == TargetType.Game);
					case ModuleRules.PrecompileTargetsType.Editor:
						return (Target.Type == TargetType.Editor);
					case ModuleRules.PrecompileTargetsType.Any:
						return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Prepares a module for building a low level tests executable.
		/// If we're building a module as part of a test module chain and there's a Tests folder with low level tests, then they require the LowLevelTestsRunner dependency.
		/// We also keep track of any Editor, Engine and other conditionally compiled dependencies.
		/// </summary>
		internal void PrepareModuleForTests()
		{
			if (Name != "LowLevelTestsRunner" && System.IO.Directory.Exists(TestsDirectory))
			{
				if (!PrivateIncludePathModuleNames.Contains("LowLevelTestsRunner"))
				{
					PrivateIncludePathModuleNames.Add("LowLevelTestsRunner");
				}
			}
			else if (Name == "LowLevelTestsRunner")
			{
				TestTargetRules.LowLevelTestsRunnerModule = this;
			}

			// If one of these modules is in the dependency graph, we must enable their appropriate global definitions
			if (Name == "UnrealEd")
			{
				// TODO: more support code required...
				TestTargetRules.bTestsRequireEditor = false;
			}
			if (Name == "Engine")
			{
				// TODO: more support code required...
				TestTargetRules.bTestsRequireEngine = false;
			}
			if (Name == "ApplicationCore")
			{
				TestTargetRules.bTestsRequireApplicationCore = true;
			}
			if (Name == "CoreUObject")
			{
				TestTargetRules.bTestsRequireCoreUObject = true;
			}

			if (TestTargetRules.LowLevelTestsRunnerModule != null)
			{
				if (TestTargetRules.bTestsRequireEditor && !TestTargetRules.LowLevelTestsRunnerModule.PrivateDependencyModuleNames.Contains("UnrealEd"))
				{
					TestTargetRules.LowLevelTestsRunnerModule.PrivateDependencyModuleNames.Add("UnrealEd");
				}
				if (TestTargetRules.bTestsRequireEngine && !TestTargetRules.LowLevelTestsRunnerModule.PrivateDependencyModuleNames.Contains("Engine"))
				{
					TestTargetRules.LowLevelTestsRunnerModule.PrivateDependencyModuleNames.Add("Engine");
				}
				if (TestTargetRules.bTestsRequireApplicationCore && !TestTargetRules.LowLevelTestsRunnerModule.PrivateDependencyModuleNames.Contains("ApplicationCore"))
				{
					TestTargetRules.LowLevelTestsRunnerModule.PrivateDependencyModuleNames.Add("ApplicationCore");
				}
				if (TestTargetRules.bTestsRequireCoreUObject && !TestTargetRules.LowLevelTestsRunnerModule.PrivateDependencyModuleNames.Contains("CoreUObject"))
				{
					TestTargetRules.LowLevelTestsRunnerModule.PrivateDependencyModuleNames.Add("CoreUObject");
				}
			}
		}

		internal bool IsTestModule
		{
			get { return bIsTestModuleOverride ?? false; }
		}
		/// <summary>
		/// Whether this is a low level tests module.
		/// </summary>
		protected bool? bIsTestModuleOverride;

		/// <summary>
		/// Returns the module directory for a given subclass of the module (platform extensions add subclasses of ModuleRules to add in platform-specific settings)
		/// </summary>
		/// <param name="Type">typeof the subclass</param>
		/// <returns>Directory where the subclass's .Build.cs lives, or null if not found</returns>
		public DirectoryReference? GetModuleDirectoryForSubClass(Type Type)
		{
			if (DirectoriesForModuleSubClasses == null)
			{
				return null;
			}

			DirectoryReference? Directory;
			if (DirectoriesForModuleSubClasses.TryGetValue(Type, out Directory))
			{
				return Directory;
			}
			return null;
		}

		/// <summary>
		/// Returns the directories for all subclasses of this module, as well as any additional directories specified by the rules
		/// </summary>
		/// <returns>List of directories, or null if none were added</returns>
		public DirectoryReference[] GetAllModuleDirectories()
		{
			List<DirectoryReference> AllDirectories = new List<DirectoryReference> { Directory };
			AllDirectories.AddRange(AdditionalModuleDirectories);

			if (DirectoriesForModuleSubClasses != null)
			{
				AllDirectories.AddRange(DirectoriesForModuleSubClasses.Values);
			}

			return AllDirectories.ToArray();
		}

		/// <summary>
		/// Adds an additional module directory, if it exists (useful for NotForLicensees/NoRedist)
		/// </summary>
		/// <param name="Directory"></param>
		/// <returns>true if the directory exists</returns>
		protected bool ConditionalAddModuleDirectory(DirectoryReference Directory)
		{
			if (DirectoryReference.Exists(Directory))
			{
				AdditionalModuleDirectories.Add(Directory);
				return true;
			}

			return false;
		}

		/// <summary>
		/// Returns if VcPkg is supported for the build configuration.
		/// </summary>
		/// <returns>True if supported</returns>
		public bool IsVcPackageSupported
		{
			get
			{
				return Target.Platform == UnrealTargetPlatform.Win64 ||
					Target.Platform == UnrealTargetPlatform.Linux ||
					Target.Platform == UnrealTargetPlatform.LinuxArm64 ||
					Target.Platform == UnrealTargetPlatform.Mac;
			}
		}

		/// <summary>
		/// Returns the VcPkg root directory for the build configuration
		/// </summary>
		/// <param name="PackageName">The name of the third-party package</param>
		/// <returns></returns>
		public string GetVcPackageRoot(string PackageName)
		{
			// TODO: MacOS support, other platform support
			string TargetPlatform = Target.Platform.ToString();
			string? Platform = null;
			string? Architecture = null;
			string Linkage = string.Empty;
			string Toolset = string.Empty;
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				Platform = "windows";
				Architecture = Target.WindowsPlatform.Architecture.ToString().ToLowerInvariant();
				if (Target.bUseStaticCRT)
				{
					Linkage = "-static";
				}
				else
				{
					Linkage = "-static-md";
				}
				Toolset = "-v142";
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux)
			{
				Architecture = "x86_64";
				Platform = "unknown-linux-gnu";
			}
			else if (Target.Platform == UnrealTargetPlatform.LinuxArm64)
			{
				Architecture = "aarch64";
				Platform = "unknown-linux-gnueabi";
			}
			else if (Target.Platform == UnrealTargetPlatform.Mac)
			{
				Architecture = "x86_64";
				Platform = "osx";
			}

			if (string.IsNullOrEmpty(TargetPlatform) || string.IsNullOrEmpty(Platform) || string.IsNullOrEmpty(Architecture))
			{
				throw new System.NotSupportedException($"Platform {Target.Platform} not currently supported by vcpkg");
			}

			string Triplet = $"{Architecture}-{Platform}{Linkage}{Toolset}";

			return Path.Combine("ThirdParty", "vcpkg", TargetPlatform, Triplet, $"{PackageName}_{Triplet}");
		}

		/// <summary>
		/// Adds libraries compiled with vcpkg to the current module
		/// </summary>
		/// <param name="PackageName">The name of the third-party package</param>
		/// <param name="AddInclude">Should the include directory be added to PublicIncludePaths</param>
		/// <param name="Libraries">The names of the libaries to add to PublicAdditionalLibraries/</param>
		public void AddVcPackage(string PackageName, bool AddInclude, params string[] Libraries)
		{
			string VcPackageRoot = GetVcPackageRoot(PackageName);

			if (!System.IO.Directory.Exists(VcPackageRoot))
			{
				throw new DirectoryNotFoundException(VcPackageRoot);
			}

			string LibraryExtension = string.Empty;
			if (Target.Platform == UnrealTargetPlatform.Win64)
			{
				LibraryExtension = ".lib";
			}
			else if (Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.LinuxArm64 || Target.Platform == UnrealTargetPlatform.Mac)
			{
				LibraryExtension = ".a";
			}

			foreach (string Library in Libraries)
			{
				string LibraryPath = Path.Combine(VcPackageRoot, "lib", $"{Library}{LibraryExtension}");
				if ((Target.Platform == UnrealTargetPlatform.Linux || Target.Platform == UnrealTargetPlatform.LinuxArm64 || Target.Platform == UnrealTargetPlatform.Mac) && !Library.StartsWith("lib"))
				{
					LibraryPath = Path.Combine(VcPackageRoot, "lib", $"lib{Library}{LibraryExtension}");
				}
				if (!System.IO.File.Exists(LibraryPath))
				{
					throw new FileNotFoundException(LibraryPath);
				}
				PublicAdditionalLibraries.Add(LibraryPath);
			}

			if (AddInclude)
			{
				string IncludePath = Path.Combine(VcPackageRoot, "include");
				if (!System.IO.Directory.Exists(IncludePath))
				{
					throw new DirectoryNotFoundException(IncludePath);
				}

				PublicSystemIncludePaths.Add(Path.Combine(VcPackageRoot, "include"));
			}
		}

		/// <summary>
		/// Replace an expected value in a list of definitions with a new value
		/// </summary>
		/// <param name="Definitions">List of definitions e.g. PublicDefinitions</param>
		/// <param name="Name">Name of the define to change</param>
		/// <param name="PreviousValue">Expected value</param>
		/// <param name="NewValue">New value</param>
		/// <exception cref="Exception"></exception>
		protected static void ChangeDefinition(List<string> Definitions, string Name, string PreviousValue, string NewValue)
		{
			if (!Definitions.Remove($"{Name}={PreviousValue}"))
			{
				throw new Exception("Failed to removed expected definition");
			}
			Definitions.Add($"{Name}={NewValue}");
		}

		/// <summary>
		/// Replace an expected value in a list of module names with a new value
		/// </summary>
		/// <param name="Definitions">List of module names e.g. PublicDependencyModuleNames</param>
		/// <param name="PreviousModule">Expected value</param>
		/// <param name="NewModule">New value</param>
		/// <exception cref="Exception"></exception>
		protected static void ReplaceModule(List<string> Definitions, string PreviousModule, string NewModule)
		{
			if (!Definitions.Remove(PreviousModule))
			{
				throw new Exception("Failed to removed expected module name");
			}
			Definitions.Add(NewModule);
		}
	}
}
