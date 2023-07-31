// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using EpicGames.Core;
using System.Diagnostics.CodeAnalysis;
using UnrealBuildBase;
using System.Runtime.Versioning;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
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
		/// Use the Intel oneAPI C++ compiler
		/// </summary>
		Intel,

		/// <summary>
		/// Visual Studio 2019 (Visual C++ 16.0)
		/// </summary>
		VisualStudio2019,

		/// <summary>
		/// Visual Studio 2022 (Visual C++ 17.0)
		/// </summary>
		VisualStudio2022,
	}


	/// <summary>
	/// Extension methods for WindowsCompilier enum
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
			return Compiler == WindowsCompiler.Clang || Compiler == WindowsCompiler.Intel;
		}

		/// <summary>
		/// Returns if this compiler toolchain based on MSVC
		/// </summary>
		/// <param name="Compiler">The compiler to check</param>
		/// <returns>true if MSVC based</returns>
		public static bool IsMSVC(this WindowsCompiler Compiler)
		{
			return Compiler >= WindowsCompiler.VisualStudio2019;
		}
	}


	/// <summary>
	/// Which static analyzer to use
	/// </summary>
	[Obsolete("Replace with StaticAnalyzer. Will be removed in 5.2")]
	public enum WindowsStaticAnalyzer
	{
		/// <summary>
		/// Do not perform static analysis
		/// </summary>
		None,

		/// <summary>
		/// Use the default static analyzer for the selected compiler, if it has one. For
		/// Visual Studio and Clang, this means using their built-in static analysis tools.
		/// Any compiler that doesn't support static analysis will ignore this option.
		/// </summary>
		Default, 

		/// <summary>
		/// Use the built-in Visual C++ static analyzer
		/// </summary>
		VisualCpp = Default,

		/// <summary>
		/// Use PVS-Studio for static analysis
		/// </summary>
		PVSStudio,
		
		/// <summary>
		/// Use clang for static analysis. This forces the compiler to clang.
		/// </summary>
		Clang,
	}

	/// <summary>
	/// Output type for the static analyzer. This currently only works for the Clang static analyzer.
	/// The Clang static analyzer can do either Text, which prints the analysis to stdout, or
	/// html, where it writes out a navigable HTML page for each issue that it finds, per file.
	/// The HTML is output in the same directory as the object fil that would otherwise have
	/// been generated. 
	/// All other analyzers default automatically to Text. 
	/// </summary>
	[Obsolete("Replace with StaticAnalyzerOutputType. Will be removed in 5.2")]
	public enum WindowsStaticAnalyzerOutputType
	{
		/// <summary>
		/// Output the analysis to stdout.
		/// </summary>
		Text,

		/// <summary>
		/// Output the analysis to an HTML file in the object folder.
		/// </summary>
		Html,
	}

	/// <summary>
	/// Available architectures on Windows platform
	/// </summary>
	public enum WindowsArchitecture
	{
		/// <summary>
		/// x64
		/// </summary>
		x64,
		/// <summary>
		/// ARM64
		/// </summary>
		ARM64,
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
		/// Version of the compiler toolchain to use on Windows platform. A value of "default" will be changed to a specific version at UBT start up.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "Compiler")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-2019", Value = nameof(WindowsCompiler.VisualStudio2019))]
		[CommandLine("-2022", Value = nameof(WindowsCompiler.VisualStudio2022))]
		[CommandLine("-Compiler=")]
		public WindowsCompiler Compiler = WindowsCompiler.Default;

		/// <summary>
		/// Version of the toolchain to use on Windows platform when a non-msvc Compiler is in use, to locate include paths etc.
		/// </summary>
		public WindowsCompiler ToolChain => Compiler.IsMSVC() ? Compiler : WindowsPlatform.GetDefaultCompiler(Target.ProjectFile, Architecture, Target.Logger);

		/// <summary>
		/// Architecture of Target.
		/// </summary>
		public WindowsArchitecture Architecture
		{
			get;
			internal set;
		}
		= WindowsArchitecture.x64;

		/// <summary>
		/// The specific toolchain version to use. This may be a specific version number (for example, "14.13.26128"), the string "Latest" to select the newest available version, or
		/// the string "Preview" to select the newest available preview version. By default, and if it is available, we use the toolchain version indicated by
		/// WindowsPlatform.DefaultToolChainVersion (otherwise, we use the latest version).
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/WindowsTargetPlatform.WindowsTargetSettings", "CompilerVersion")]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-CompilerVersion")]
		public string? CompilerVersion = null;

		/// <summary>
		/// The specific Windows SDK version to use. This may be a specific version number (for example, "8.1", "10.0" or "10.0.10150.0"), or the string "Latest", to select the newest available version.
		/// By default, and if it is available, we use the Windows SDK version indicated by WindowsPlatform.DefaultWindowsSdkVersion (otherwise, we use the latest version).
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		public string? WindowsSdkVersion = null;

		/// <summary>
		/// Value for the WINVER macro, defining the minimum supported Windows version.
		/// </summary>
		public int TargetWindowsVersion = 0x601;

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
		/// The static analyzer to use.
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-StaticAnalyzer")]
		[Obsolete("Replace with TargetRuies.StaticAnalyzer (Xml Category BuildConfiguration). Will be removed in 5.2")]
		public WindowsStaticAnalyzer StaticAnalyzer = WindowsStaticAnalyzer.None;

		/// <summary>
		/// The output type to use for the static analyzer.
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-StaticAnalyzerOutputType")]
		[Obsolete("Replace with TargetRuies.StaticAnalyzerOutputType (Xml Category BuildConfiguration). Will be removed in 5.2")]
		public WindowsStaticAnalyzerOutputType StaticAnalyzerOutputType = WindowsStaticAnalyzerOutputType.Text;

		/// <summary>
		/// Enables address sanitizer (ASan). Only supported for Visual Studio 2019 16.7.0 and up.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableAddressSanitizer")]
		[CommandLine("-EnableASan")]
		public bool bEnableAddressSanitizer = false;

		/// <summary>
		/// Whether we should export a file containing .obj to source file mappings.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-ObjSrcMap")]
		public string? ObjSrcMapFile = null;

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
		/// Enables strict standard conformance mode (/permissive-).
		/// </summary>
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-Strict")]
		public bool bStrictConformanceMode = false;

		/// <summary>
		/// Enables updated __cplusplus macro (/Zc:__cplusplus).
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-UpdatedCPPMacro")]
		public bool bUpdatedCPPMacro = false;

		/// <summary>
		/// Enables inline conformance (Remove unreferenced COMDAT) (/Zc:inline).
		/// </summary>
		[RequiresUniqueBuildEnvironment]
		[XmlConfigFile(Category = "WindowsPlatform")]
		[CommandLine("-StrictInline")]
		public bool bStrictInlineConformance = false;

		/// <summary>
		/// Enables new preprocessor conformance (/Zc:preprocessor).
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

		/// VS2015 updated some of the CRT definitions but not all of the Windows SDK has been updated to match.
		/// Microsoft provides legacy_stdio_definitions library to enable building with VS2015 until they fix everything up.
		public bool bNeedsLegacyStdioDefinitionsLib
		{
			get { return Compiler.IsMSVC() || Compiler.IsClang(); }
		}

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
			get { return bCreateHotPatchableImagePrivate ?? Target.bWithLiveCoding; }
			set { bCreateHotPatchableImagePrivate = value; }
		}
		private bool? bCreateHotPatchableImagePrivate;

		/// <summary>
		/// Strip unreferenced symbols (/OPT:REF)
		/// </summary>
		public bool bStripUnreferencedSymbols
		{
			get { return bStripUnreferencedSymbolsPrivate ?? ((Target.Configuration == UnrealTargetConfiguration.Test || Target.Configuration == UnrealTargetConfiguration.Shipping) && !Target.bWithLiveCoding); }
			set { bStripUnreferencedSymbolsPrivate = value; }
		}
		private bool? bStripUnreferencedSymbolsPrivate;
			
		/// <summary>
		/// Merge identical COMDAT sections together (/OPT:ICF)
		/// </summary>
		public bool bMergeIdenticalCOMDATs
		{
			get { return bMergeIdenticalCOMDATsPrivate ?? ((Target.Configuration == UnrealTargetConfiguration.Test || Target.Configuration == UnrealTargetConfiguration.Shipping) && !Target.bWithLiveCoding); }
			set { bMergeIdenticalCOMDATsPrivate = value; }
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
		/// Settings for PVS studio
		/// </summary>
		public PVSTargetSettings PVS = new PVSTargetSettings();

		/// <summary>
		/// The Visual C++ environment to use for this target. Only initialized after all the target settings are finalized, in ValidateTarget().
		/// </summary>
		internal VCEnvironment? Environment;

		/// <summary>
		/// Directory containing the toolchain
		/// </summary>
		public string? ToolChainDir
		{
			get { return (Environment == null)? null : Environment.ToolChainDir.FullName; }
		}

		/// <summary>
		/// The version number of the toolchain
		/// </summary>
		public string? ToolChainVersion
		{
			get { return (Environment == null)? null : Environment.ToolChainVersion.ToString(); }
		}

		/// <summary>
		/// Root directory containing the Windows Sdk
		/// </summary>
		public string? WindowsSdkDir
		{
			get { return (Environment == null)? null : Environment.WindowsSdkDir.FullName; }
		}


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
		public string? DiaSdkDir
		{
			get { return MicrosoftPlatformSDK.FindDiaSdkDirs(Environment!.Compiler).Select(x => x.FullName).FirstOrDefault(); }
		}

		/// <summary>
		/// Directory containing the IDE package (Professional, Community, etc...)
		/// </summary>
		public string? IDEDir
		{
			get
			{
				try
				{
					return MicrosoftPlatformSDK.FindVisualStudioInstallations(Environment!.Compiler, Target.Logger).Select(x => x.BaseDir.FullName).FirstOrDefault();
				}
				catch(Exception) // Find function will throw if there is no visual studio installed! This can happen w/ clang builds
				{
					return null;
				}
			}
		}

		/// <summary>
		/// When using a Visual Studio compiler, returns the version name as a string
		/// </summary>
		/// <returns>The Visual Studio compiler version name (e.g. "2019")</returns>
		public string GetVisualStudioCompilerVersionName()
		{
			switch (Compiler)
			{
				case WindowsCompiler.Clang:
				case WindowsCompiler.Intel:
				case WindowsCompiler.VisualStudio2019:
				case WindowsCompiler.VisualStudio2022:
					return "2015"; // VS2022 is backwards compatible with VS2015 compiler

				default:
					throw new BuildException("Unexpected WindowsCompiler version for GetVisualStudioCompilerVersionName().  Either not using a Visual Studio compiler or switch block needs to be updated");
			}
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Target">The target rules which owns this object</param>
		internal WindowsTargetRules(TargetRules Target)
		{
			this.Target = Target;

			ManifestFile = FileReference.Combine(Unreal.EngineDirectory, "Build", "Windows", "Resources", String.Format("Default-{0}.manifest", Target.Platform)).FullName;
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
			this.PVS = new ReadOnlyPVSTargetSettings(Inner.PVS);
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
		
		public WindowsArchitecture Architecture
		{
			get { return Inner.Architecture; }
		}

		public string? CompilerVersion
		{
			get { return Inner.CompilerVersion; }
		}

		public string? WindowsSdkVersion
		{
			get { return Inner.WindowsSdkVersion; }
		}

		public int TargetWindowsVersion
		{
			get { return Inner.TargetWindowsVersion; }
		}

		public bool bPixProfilingEnabled
		{
			get { return Inner.bPixProfilingEnabled; }
		}

		public bool bUseWindowsSDK10
		{
			get { return Inner.bUseWindowsSDK10; }
		}

		public bool bUseCPPWinRT
		{
			get { return Inner.bUseCPPWinRT; }
		}

		public bool bEnableRayTracing
		{
			get { return Inner.bEnableRayTracing; }
		}

		public string? CompanyName
		{
			get { return Inner.CompanyName; }
		}

		public string? CopyrightNotice
		{
			get { return Inner.CopyrightNotice; }
		}

		public string? ProductName
		{
			get { return Inner.ProductName; }
		}

		[Obsolete("Replace with TargetRuies.StaticAnalyzer (Xml Category BuildConfiguration). Will be removed in 5.2")]
		public WindowsStaticAnalyzer StaticAnalyzer
		{
			get { return Inner.StaticAnalyzer; }
		}

		[Obsolete("Replace with TargetRuies.StaticAnalyzerOutputType (Xml Category BuildConfiguration). Will be removed in 5.2")]
		public WindowsStaticAnalyzerOutputType StaticAnalyzerOutputType
		{
			get { return Inner.StaticAnalyzerOutputType; }
		}

		public bool bEnableAddressSanitizer
		{ 
			get { return Inner.bEnableAddressSanitizer; }
		}

		public string? ObjSrcMapFile
		{
			get { return Inner.ObjSrcMapFile; }
		}

		public string? ModuleDefinitionFile
		{
			get { return Inner.ModuleDefinitionFile; }
		}

		public string? ManifestFile
		{
			get { return Inner.ManifestFile; }
		}

		public bool bNeedsLegacyStdioDefinitionsLib
		{
			get { return Inner.bNeedsLegacyStdioDefinitionsLib; }
		}

		public bool bStrictConformanceMode
		{
			get { return Inner.bStrictConformanceMode; }
		}

		public bool bUpdatedCPPMacro
		{
			get { return Inner.bUpdatedCPPMacro; }
		}

		public bool bStrictInlineConformance
		{
			get { return Inner.bStrictInlineConformance; }
		}

		public bool bStrictPreprocessorConformance
		{
			get { return Inner.bStrictPreprocessorConformance; }
		}

		public bool bStrictEnumTypesConformance
		{
			get { return Inner.bStrictEnumTypesConformance; }
		}

		public int DefaultStackSize
		{
			get { return Inner.DefaultStackSize; }
		}

		public int DefaultStackSizeCommit
		{
			get { return Inner.DefaultStackSizeCommit; }
		}

		public int PCHMemoryAllocationFactor
		{
			get { return Inner.PCHMemoryAllocationFactor; }
		}

		public string AdditionalLinkerOptions
		{
			get { return Inner.AdditionalLinkerOptions; }
		}

		public bool bCreateHotpatchableImage
		{
			get { return Inner.bCreateHotPatchableImage; }
		}

		public bool bStripUnreferencedSymbols
		{
			get { return Inner.bStripUnreferencedSymbols; }
		}

		public bool bMergeIdenticalCOMDATs
		{
			get { return Inner.bMergeIdenticalCOMDATs; }
		}

		public bool bOptimizeGlobalData
		{
			get { return Inner.bOptimizeGlobalData; }
		}

		public bool bClangTimeTrace
		{
			get { return Inner.bClangTimeTrace; }
		}

		public bool bCompilerTrace
		{
			get { return Inner.bCompilerTrace; }
		}

		public bool bShowIncludes
		{
			get { return Inner.bShowIncludes; }
		}

		public string GetVisualStudioCompilerVersionName()
		{
			return Inner.GetVisualStudioCompilerVersionName();
		}

		public bool bUseBundledDbgHelp
		{
			get { return Inner.bUseBundledDbgHelp; }
		}

		public ReadOnlyPVSTargetSettings PVS
		{
			get; private set;
		}

		internal VCEnvironment? Environment
		{
			get { return Inner.Environment; }
		}

		public string? ToolChainDir
		{
			get { return Inner.ToolChainDir; }
		}

		public string? ToolChainVersion
		{
			get { return Inner.ToolChainVersion; }
		}

		public string? WindowsSdkDir
		{
			get { return Inner.WindowsSdkDir; }
		}

		public string? NetFxSdkDir
		{
			get { return Inner.NetFxSdkDir; }
		}

		public string? DiaSdkDir
		{
			get { return Inner.DiaSdkDir; }
		}
		
		public string? IDEDir
		{
			get { return Inner.IDEDir; }
		}

		public string GetArchitectureSubpath()
		{
			return WindowsExports.GetArchitectureSubpath(Architecture);
		}

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
		/// Whether it's a pre-release version of the IDE.
		/// </summary>
		public bool bPreview { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public VisualStudioInstallation(WindowsCompiler Compiler, VersionNumber Version, DirectoryReference BaseDir, bool bCommunity, bool bPreview)
		{
			this.Compiler = Compiler;
			this.Version = Version;
			this.BaseDir = BaseDir;
			this.bCommunity = bCommunity;
			this.bPreview = bPreview;
		}
	}

	class WindowsPlatform : UEBuildPlatform
	{
		/// <summary>
		/// True if we should use the Clang linker (LLD) when we are compiling with Clang, otherwise we use the MSVC linker
		/// </summary>
		public static readonly bool bAllowClangLinker = false;

		/// <summary>
		/// True if we should use the Intel linker (xilink\xilib) when we are compiling with Intel oneAPI, otherwise we use the MSVC linker
		/// </summary>
		public static readonly bool bAllowIntelLinker = true;
		
		MicrosoftPlatformSDK SDK;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InPlatform">Creates a windows platform with the given enum value</param>
		/// <param name="InSDK">The installed Windows SDK</param>
		/// <param name="InLogger">Logger instance</param>
		public WindowsPlatform(UnrealTargetPlatform InPlatform, MicrosoftPlatformSDK InSDK, ILogger InLogger)
			: base(InPlatform, InSDK, InLogger)
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
			return VCEnvironment.Create(Target.WindowsPlatform.Compiler, Target.WindowsPlatform.ToolChain, Platform, Target.WindowsPlatform.Architecture, Target.WindowsPlatform.CompilerVersion, Target.WindowsPlatform.WindowsSdkVersion, null, Target.WindowsPlatform.bUseCPPWinRT, Logger);
		}

		/// <summary>
		/// Validate a target's settings
		/// </summary>
		[SupportedOSPlatform("windows")]
		public override void ValidateTarget(TargetRules Target)
		{
			if (Platform == UnrealTargetPlatform.Win64)
			{
				Target.WindowsPlatform.Architecture = WindowsArchitecture.x64;
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
				Target.WindowsPlatform.Compiler = WindowsCompiler.Clang;
				Target.StaticAnalyzer = StaticAnalyzer.Default;
			}
			else if (Target.StaticAnalyzer != StaticAnalyzer.None && 
			         Target.StaticAnalyzerOutputType != StaticAnalyzerOutputType.Text)
			{
				Logger.LogInformation("Defaulting static analyzer output type to text");
			}

			// Set the compiler version if necessary
			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Default)
			{
				Target.WindowsPlatform.Compiler = GetDefaultCompiler(Target.ProjectFile, Target.WindowsPlatform.Architecture, Logger);
			}

			// Disable linking and ignore build outputs if we're using a static analyzer
			if (Target.StaticAnalyzer != StaticAnalyzer.None)
			{
				Target.bDisableLinking = true;
				Target.bIgnoreBuildOutputs = true;
			}

			// Disable PCHs for PVS studio analyzer.
			if (Target.StaticAnalyzer == StaticAnalyzer.PVSStudio)
			{
				Target.bUsePCHFiles = false;
			}
			
			// @todo: Override PCH settings
			if (Target.WindowsPlatform.Compiler == WindowsCompiler.Intel)
			{
				Target.bUseSharedPCHs = false;
				Target.bUsePCHFiles = false;
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

			// If we're using PDB files and PCHs, the generated code needs to be compiled with the same options as the PCH.
			if ((Target.bUsePDBFiles || Target.bSupportEditAndContinue) && Target.bUsePCHFiles)
			{
				Target.bDisableDebugInfoForGeneratedCode = false;
			}

			Target.bCompileISPC = true;

			// Initialize the VC environment for the target, and set all the version numbers to the concrete values we chose
			Target.WindowsPlatform.Environment = CreateVCEnvironment(Target);

			// pull some things from it
			Target.WindowsPlatform.Compiler = Target.WindowsPlatform.Environment.Compiler;
			Target.WindowsPlatform.CompilerVersion = Target.WindowsPlatform.Environment.CompilerVersion.ToString();
			Target.WindowsPlatform.WindowsSdkVersion = Target.WindowsPlatform.Environment.WindowsSdkVersion.ToString();

			// If we're enabling support for C++ modules, make sure the compiler supports it. VS 16.8 changed which command line arguments are used to enable modules support.
			if (Target.bEnableCppModules && !ProjectFileGenerator.bGenerateProjectFiles && Target.WindowsPlatform.Environment.CompilerVersion < new VersionNumber(14, 28, 29304))
			{
				throw new BuildException("Support for C++20 modules requires Visual Studio 2019 16.8 preview 3 or later. Compiler Version Targeted: {0}", Target.WindowsPlatform.Environment.CompilerVersion);
			}

			// Ensure we're using recent enough version of Visual Studio to support ASan builds.
			if (Target.WindowsPlatform.bEnableAddressSanitizer && Target.WindowsPlatform.Environment.CompilerVersion < new VersionNumber(14, 27, 0))
			{
				throw new BuildException("Address sanitizer requires Visual Studio 2019 16.7 or later.");
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
		internal static WindowsCompiler GetDefaultCompiler(FileReference? ProjectFile, WindowsArchitecture Architecture, ILogger Logger)
		{
			// If there's no specific compiler set, try to pick the matching compiler for the selected IDE
			if (ProjectFileGeneratorSettings.Format != null)
			{
				foreach(ProjectFileFormat Format in ProjectFileGeneratorSettings.ParseFormatList(ProjectFileGeneratorSettings.Format, Logger))
				{
					if (Format == ProjectFileFormat.VisualStudio2019)
					{
						return WindowsCompiler.VisualStudio2019;
					}
					else if (Format == ProjectFileFormat.VisualStudio2022)
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
				if (ProjectFormat == VCProjectFileFormat.VisualStudio2019)
				{
					return WindowsCompiler.VisualStudio2019;
				}
				else if (ProjectFormat == VCProjectFileFormat.VisualStudio2022)
				{
					return WindowsCompiler.VisualStudio2022;
				}
			}

			// Check the editor settings too
			ProjectFileFormat PreferredAccessor;
			if(ProjectFileGenerator.GetPreferredSourceCodeAccessor(ProjectFile, out PreferredAccessor))
			{
				if(PreferredAccessor == ProjectFileFormat.VisualStudio2019)
			    {
				    return WindowsCompiler.VisualStudio2019;
			    }
				else if (PreferredAccessor == ProjectFileFormat.VisualStudio2022)
				{
					return WindowsCompiler.VisualStudio2022;
				}
			}

			// Second, default based on what's installed, test for 2019 first
			if (MicrosoftPlatformSDK.HasValidCompiler(WindowsCompiler.VisualStudio2019, Architecture, Logger))
			{
				return WindowsCompiler.VisualStudio2019;
			}
			else if (MicrosoftPlatformSDK.HasValidCompiler(WindowsCompiler.VisualStudio2022, Architecture, Logger))
			{
				return WindowsCompiler.VisualStudio2022;
			}

			// If we do have a Visual Studio installation, but we're missing just the C++ parts, warn about that.
			if (TryGetVSInstallDirs(WindowsCompiler.VisualStudio2019, Logger) != null)
			{
				string ToolSetWarning = Architecture == WindowsArchitecture.x64 ?
					"MSVC v142 - VS 2019 C++ x64/x86 build tools (Latest)" :
					"MSVC v142 - VS 2019 C++ ARM64 build tools (Latest)";
				Logger.LogWarning("Visual Studio 2019 is installed, but is missing the C++ toolchain. Please verify that the \"{Component}\" component is selected in the Visual Studio 2019 installation options.", ToolSetWarning);
			}
			else if (TryGetVSInstallDirs(WindowsCompiler.VisualStudio2022, Logger) != null)
			{
				string ToolSetWarning = Architecture == WindowsArchitecture.x64 ?
					"MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)" :
					"MSVC v143 - VS 2022 C++ ARM64 build tools (Latest)";
				Logger.LogWarning("Visual Studio 2022 is installed, but is missing the C++ toolchain. Please verify that the \"{Component}\" component is selected in the Visual Studio 2022 installation options.", ToolSetWarning);
			}
			else
			{
				Logger.LogWarning("No Visual C++ installation was found. Please download and install Visual Studio 2019 or 2022 with C++ components.");
			}

			// Finally, default to VS2019 anyway
			return WindowsCompiler.VisualStudio2019;
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
			if(Installations.Count == 0)
			{
				return null;
			}

			return Installations.Select(x => x.BaseDir);
		}

		/// <summary>
		/// Determines if a given compiler is installed
		/// </summary>
		/// <param name="Compiler">Compiler to check for</param>
		/// <param name="Architecture">Architecture the compiler must support</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>True if the given compiler is installed</returns>
		public static bool HasCompiler(WindowsCompiler Compiler, WindowsArchitecture Architecture, ILogger Logger)
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
		public static bool TryGetToolChainDir(WindowsCompiler Compiler, string? CompilerVersion, WindowsArchitecture Architecture, ILogger Logger, [NotNullWhen(true)] out VersionNumber? OutToolChainVersion, [NotNullWhen(true)] out DirectoryReference? OutToolChainDir, out DirectoryReference? OutRedistDir)
		{
			return MicrosoftPlatformSDK.TryGetToolChainDir(Compiler, CompilerVersion, Architecture, Logger, out OutToolChainVersion, out OutToolChainDir, out OutRedistDir);
		}

		public static string GetArchitectureSubpath(WindowsArchitecture arch)
		{
			string archPath = "Unknown";
			if (arch == WindowsArchitecture.x64)
			{
				archPath = "x64";
			}
			else if (arch == WindowsArchitecture.ARM64)
			{
				archPath = "arm64";
			}
			return archPath;
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
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".lib")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".pdb")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".exp")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".obj")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".map")
				|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".objpaths");
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
					return new string[] {".pdb"};
			}
			return new string [] {};
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
			if(ProjectFile != null)
			{
				FileReference IconFile = FileReference.Combine(ProjectFile.Directory, "Build", "Windows", "Application.ico");
				if(FileReference.Exists(IconFile))
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

			if (!Target.bBuildRequiresCookedData)
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

			if (ModuleName == "D3D11RHI")
			{
				// To enable platform specific D3D11 RHI Types
				Rules.PrivateIncludePaths.Add("Runtime/Windows/D3D11RHI/Private/Windows");
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
			
			CompileEnvironment.Definitions.Add("PLATFORM_WINDOWS=1");
			CompileEnvironment.Definitions.Add("PLATFORM_MICROSOFT=1");

			// both Win32 and Win64 use Windows headers, so we enforce that here
			CompileEnvironment.Definitions.Add(String.Format("OVERRIDE_PLATFORM_HEADER_NAME={0}", GetPlatformName()));

			// Ray tracing only supported on 64-bit Windows.
			if (Target.Platform == UnrealTargetPlatform.Win64 && Target.WindowsPlatform.bEnableRayTracing)
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

			if ((Target.bPGOOptimize || Target.bPGOProfile) && Target.ProjectFile != null)
			{
				// LTCG is required for PGO
				//CompileEnvironment.bAllowLTCG = true;
				//LinkEnvironment.bAllowLTCG = true;

				CompileEnvironment.PGODirectory = Path.Combine(DirectoryReference.FromFile(Target.ProjectFile).FullName, "Platforms", "Windows", "Build", "PGO");
				CompileEnvironment.PGOFilenamePrefix = string.Format("{0}-{1}-{2}", Target.Name, Target.Platform, Target.Configuration);

				LinkEnvironment.PGODirectory = CompileEnvironment.PGODirectory;
				LinkEnvironment.PGOFilenamePrefix = CompileEnvironment.PGOFilenamePrefix;
			}
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
			if (Target.StaticAnalyzer == StaticAnalyzer.PVSStudio)
			{
				return new PVSToolChain(Target, Logger);
			}
			else
			{
				return new VCToolChain(Target, Logger);
			}
		}

		/// <summary>
		/// Allows the platform to return various build metadata that is not tracked by other means. If the returned string changes, the makefile will be invalidated.
		/// </summary>
		/// <param name="ProjectFile">The project file being built</param>
		/// <param name="Metadata">String builder to contain build metadata</param>
		public override void GetExternalBuildMetadata(FileReference? ProjectFile, StringBuilder Metadata)
		{
			base.GetExternalBuildMetadata(ProjectFile, Metadata);

			if(ProjectFile != null)
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
		public override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Win64; }
		}

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
