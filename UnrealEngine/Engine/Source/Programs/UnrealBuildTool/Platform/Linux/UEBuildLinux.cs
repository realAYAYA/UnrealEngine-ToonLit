// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using EpicGames.Core;
using System.Text.RegularExpressions;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/** Architecture as stored in the ini. */
	enum LinuxArchitecture
	{
		/** x86_64, most commonly used architecture.*/
		X86_64UnknownLinuxGnu,

		/** A.k.a. AArch32, ARM 32-bit with hardware floats */
		ArmUnknownLinuxGnueabihf,

		/** Arm64, ARM 64-bit */
		AArch64UnknownLinuxGnueabi,
	}

	/// <summary>
	/// Linux-specific target settings
	/// </summary>
	public class LinuxTargetRules
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public LinuxTargetRules()
		{
			XmlConfig.ApplyTo(this);
		}

		/// <summary>
		/// Enables address sanitizer (ASan)
		/// </summary>
		[CommandLine("-EnableASan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableAddressSanitizer")]
		public bool bEnableAddressSanitizer = false;

		/// <summary>
		/// Enables thread sanitizer (TSan)
		/// </summary>
		[CommandLine("-EnableTSan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableThreadSanitizer")]
		public bool bEnableThreadSanitizer = false;

		/// <summary>
		/// Enables undefined behavior sanitizer (UBSan)
		/// </summary>
		[CommandLine("-EnableUBSan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableUndefinedBehaviorSanitizer")]
		public bool bEnableUndefinedBehaviorSanitizer = false;

		/// <summary>
		/// Enables memory sanitizer (MSan)
		/// </summary>
		[CommandLine("-EnableMSan")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bEnableMemorySanitizer")]
		public bool bEnableMemorySanitizer = false;

		/// <summary>
		/// Whether or not to preserve the portable symbol file produced by dump_syms
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/LinuxPlatform.LinuxTargetSettings")]
		public bool bPreservePSYM = false;

		/// <summary>
		/// Turns on tuning of debug info for LLDB
		/// </summary>
		[CommandLine("-EnableLLDB")]
		[XmlConfigFile(Category = "BuildConfiguration", Name = "bTuneDebugInfoForLLDB")]
		public bool bTuneDebugInfoForLLDB = false;

		/// <summary>
		/// Enables runtime ray tracing support.
		/// </summary>
		[ConfigFile(ConfigHierarchyType.Engine, "/Script/LinuxTargetPlatform.LinuxTargetSettings")]
		public bool bEnableRayTracing = false;
	}

	/// <summary>
	/// Read-only wrapper for Linux-specific target settings
	/// </summary>
	public class ReadOnlyLinuxTargetRules
	{
		private LinuxTargetRules Inner;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Inner">The settings object to wrap</param>
		public ReadOnlyLinuxTargetRules(LinuxTargetRules Inner)
		{
			this.Inner = Inner;
		}

		/// <summary>
		/// Accessors for fields on the inner TargetRules instance
		/// </summary>
		#region Read-only accessor properties 
		#pragma warning disable CS1591

		public bool bPreservePSYM
		{
			get { return Inner.bPreservePSYM; }
		}

		public bool bEnableAddressSanitizer
		{
			get { return Inner.bEnableAddressSanitizer; }
		}

		public bool bEnableThreadSanitizer
		{
			get { return Inner.bEnableThreadSanitizer; }
		}

		public bool bEnableUndefinedBehaviorSanitizer
		{
			get { return Inner.bEnableUndefinedBehaviorSanitizer; }
		}

		public bool bEnableMemorySanitizer
		{
			get { return Inner.bEnableMemorySanitizer; }
		}

		public bool bTuneDebugInfoForLLDB
		{
			get { return Inner.bTuneDebugInfoForLLDB; }
		}

		public bool bEnableRayTracing
		{
			get { return Inner.bEnableRayTracing; }
		}

		#pragma warning restore CS1591
		#endregion
	}

	class LinuxPlatform : UEBuildPlatform
	{
		/// <summary>
		/// Linux host architecture (compiler target triplet)
		/// </summary>
		public const string DefaultHostArchitecture = "x86_64-unknown-linux-gnu";

		/// <summary>
		/// SDK in use by the platform
		/// </summary>
		protected LinuxPlatformSDK SDK;

		/// <summary>
		/// Constructor
		/// </summary>
		public LinuxPlatform(LinuxPlatformSDK InSDK, ILogger Logger) 
			: this(UnrealTargetPlatform.Linux, InSDK, Logger)
		{
			SDK = InSDK;
		}

		public LinuxPlatform(UnrealTargetPlatform UnrealTarget, LinuxPlatformSDK InSDK, ILogger Logger)
			: base(UnrealTarget, InSDK, Logger)
		{
			SDK = InSDK;
		}

		/// <summary>
		/// Find the default architecture for the given project
		/// </summary>
		public override string GetDefaultArchitecture(FileReference? ProjectFile)
		{
			if (Platform == UnrealTargetPlatform.LinuxArm64)
			{
				return "aarch64-unknown-linux-gnueabi";
			}
			else
			{
				return "x86_64-unknown-linux-gnu";
			}
		}

		/// <summary>
		/// Get name for architecture-specific directories (can be shorter than architecture name itself)
		/// </summary>
		public override string GetFolderNameForArchitecture(string Architecture)
		{
			// shorten the string (heuristically)
			uint Sum = 0;
			int Len = Architecture.Length;
			for (int Index = 0; Index < Len; ++Index)
			{
				Sum += (uint)(Architecture[Index]);
				Sum <<= 1;	// allowed to overflow
			}
			return Sum.ToString("X");
		}

		public override void ResetTarget(TargetRules Target)
		{
			ValidateTarget(Target);
		}

		public override void ValidateTarget(TargetRules Target)
		{
			if (!string.IsNullOrWhiteSpace(Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE")))
			{
				Target.StaticAnalyzer = StaticAnalyzer.Default;
				Target.StaticAnalyzerOutputType = (Environment.GetEnvironmentVariable("CLANG_ANALYZER_OUTPUT")?.Contains("html", StringComparison.OrdinalIgnoreCase) == true) ? StaticAnalyzerOutputType.Html : StaticAnalyzerOutputType.Text;
				Target.StaticAnalyzerMode = string.Equals(Environment.GetEnvironmentVariable("CLANG_STATIC_ANALYZER_MODE"), "shallow") ? StaticAnalyzerMode.Shallow : StaticAnalyzerMode.Deep;
			}
			else if (Target.StaticAnalyzer == StaticAnalyzer.Clang)
			{
				Target.StaticAnalyzer = StaticAnalyzer.Default;
			}

			// Disable linking and ignore build outputs if we're using a static analyzer
			if (Target.StaticAnalyzer == StaticAnalyzer.Default)
			{
				Target.bDisableLinking = true;
				Target.bIgnoreBuildOutputs = true;
			}

			// Editor target types get overwritten in UEBuildTarget.cs so lets avoid adding this here. ResetTarget is called with
			// default settings for TargetRules meanings Type == Game once then Type == Editor a 2nd time when building the Editor.
			// BuildVersion string is not set at this point so we can avoid setting a Sanitizer suffix if this is the first ResetTarget
			// with an unset TargetType. This avoids creating UnrealEditor-ASan.target while the binary is UnrealEditor.
			// These need to be promoted to higher level concepts vs this hacky solution
			if (!Target.IsNameOverriden() && !String.IsNullOrEmpty(Target.BuildVersion) && Target.Type != TargetType.Editor)
			{
				string? SanitizerSuffix = null;

				if (Target.LinuxPlatform.bEnableAddressSanitizer)
				{
					SanitizerSuffix = "ASan";
				}
				else if (Target.LinuxPlatform.bEnableThreadSanitizer)
				{
					SanitizerSuffix = "TSan";
				}
				else if (Target.LinuxPlatform.bEnableUndefinedBehaviorSanitizer)
				{
					SanitizerSuffix = "UBSan";
				}
				else if (Target.LinuxPlatform.bEnableMemorySanitizer)
				{
					SanitizerSuffix = "MSan";
				}

				if (!String.IsNullOrEmpty(SanitizerSuffix))
				{
					Target.Name = Target.Name + "-" + SanitizerSuffix;
				}
			}

			if (Target.bAllowLTCG && Target.LinkType != TargetLinkType.Monolithic)
			{
				throw new BuildException("LTO (LTCG) for modular builds is not supported (lld is not currently used for dynamic libraries).");
			}

			if (Target.GlobalDefinitions.Contains("USE_NULL_RHI=1"))
			{				
				Target.bCompileCEF3 = false;
			}

			// check if OS update invalidated our build
			Target.bCheckSystemHeadersForModification = (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux);

			Target.bCompileISPC = true;
		}

		/// <summary>
		/// Allows the platform to override whether the architecture name should be appended to the name of binaries.
		/// </summary>
		/// <returns>True if the architecture name should be appended to the binary</returns>
		public override bool RequiresArchitectureSuffix()
		{
			// Linux ignores architecture-specific names, although it might be worth it to prepend architecture
			return false;
		}

		public override bool CanUseXGE()
		{
			// [RCL] 2018-05-02: disabling XGE even during a native build because the support is not ready and you can have mysterious build failures when ib_console is installed.
			// [RCL] 2018-07-10: enabling XGE for Windows to see if the crash from 2016 still persists. Please disable if you see spurious build errors that don't repro without XGE
			// [bschaefer] 2018-08-24: disabling XGE due to a bug where XGE seems to be lower casing folders names that are headers ie. misc/Header.h vs Misc/Header.h
			// [bschaefer] 2018-10-04: enabling XGE as an update in xgConsole seems to have fixed it for me
			// [bschaefer] 2018-12-17: disable XGE again, as the same issue before seems to still be happening but intermittently
			// [bschaefer] 2019-6-13: enable XGE, as the bug from before is now fixed
			return BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64;
		}

		public override bool CanUseParallelExecutor()
		{
			// No known problems with parallel executor, always use for build machines
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
			if (FileName.StartsWith("lib"))
			{
				return IsBuildProductName(FileName, 3, FileName.Length - 3, NamePrefixes, NameSuffixes, ".a")
					|| IsBuildProductName(FileName, 3, FileName.Length - 3, NamePrefixes, NameSuffixes, ".so")
					|| IsBuildProductName(FileName, 3, FileName.Length - 3, NamePrefixes, NameSuffixes, ".sym")
					|| IsBuildProductName(FileName, 3, FileName.Length - 3, NamePrefixes, NameSuffixes, ".debug");
			}
			else
			{
				return IsBuildProductName(FileName, NamePrefixes, NameSuffixes, "")
					|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".so")
					|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".a")
					|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".sym")
					|| IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".debug");
			}
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extension (i.e. 'exe' or 'dll')</returns>
		public override string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
					return ".so";
				case UEBuildBinaryType.Executable:
					return "";
				case UEBuildBinaryType.StaticLibrary:
					return ".a";
			}
			return base.GetBinaryExtension(InBinaryType);
		}

		/// <summary>
		/// Get the extensions to use for debug info for the given binary type
		/// </summary>
		/// <param name="InTarget">Rules for the target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string[]    The debug info extensions (i.e. 'pdb')</returns>
		public override string[] GetDebugInfoExtensions(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			switch (InBinaryType)
			{
				case UEBuildBinaryType.DynamicLinkLibrary:
				case UEBuildBinaryType.Executable:
					if (InTarget.LinuxPlatform.bPreservePSYM)
					{
						return new string[] {".psym", ".sym", ".debug"};
					}
					else
					{
						return new string[] {".sym", ".debug"};
					}
			}
			return new string [] {};
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
							Rules.DynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
							Rules.DynamicallyLoadedModuleNames.Add("LinuxArm64TargetPlatform");
						}
					}
				}

				// allow standalone tools to use targetplatform modules, without needing Engine
				if (Target.bForceBuildTargetPlatforms && ModuleName == "TargetPlatform")
				{
					Rules.DynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxArm64TargetPlatform");
				}
			}
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
					Rules.DynamicallyLoadedModuleNames.Add("LinuxTargetPlatform");
					Rules.DynamicallyLoadedModuleNames.Add("LinuxArm64TargetPlatform");
				}

				if (bBuildShaderFormats)
				{
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatOpenGL");
					Rules.DynamicallyLoadedModuleNames.Add("VulkanShaderFormat");
					Rules.DynamicallyLoadedModuleNames.Add("ShaderFormatVectorVM");
				}
			}
		}

		public virtual void SetUpSpecificEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			CompileEnvironment.Definitions.Add("PLATFORM_LINUX=1");
			CompileEnvironment.Definitions.Add("PLATFORM_UNIX=1");

			CompileEnvironment.Definitions.Add("LINUX=1"); // For libOGG

			// this define does not set jemalloc as default, just indicates its support
			CompileEnvironment.Definitions.Add("PLATFORM_SUPPORTS_JEMALLOC=1");

			// LinuxArm64 uses only Linux header files
			CompileEnvironment.Definitions.Add("OVERRIDE_PLATFORM_HEADER_NAME=Linux");

			CompileEnvironment.Definitions.Add("PLATFORM_LINUXARM64=" +
				(Target.Platform == UnrealTargetPlatform.LinuxArm64 ? "1" : "0"));
		}

		/// <inheritdoc/>
		public override void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment)
		{
			// During the native builds, check the system includes as well (check toolchain when cross-compiling?)
			DirectoryReference? BaseLinuxPath = SDK.GetBaseLinuxPathForArchitecture(Target.Architecture);
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux && BaseLinuxPath == null)
			{
				CompileEnvironment.SystemIncludePaths.Add(new DirectoryReference("/usr/include"));
			}

			if (CompileEnvironment.bPGOOptimize != LinkEnvironment.bPGOOptimize)
			{
				throw new BuildException("Inconsistency between PGOOptimize settings in Compile ({0}) and Link ({1}) environments",
					CompileEnvironment.bPGOOptimize,
					LinkEnvironment.bPGOOptimize
				);
			}

			if (CompileEnvironment.bPGOProfile != LinkEnvironment.bPGOProfile)
			{
				throw new BuildException("Inconsistency between PGOProfile settings in Compile ({0}) and Link ({1}) environments",
					CompileEnvironment.bPGOProfile,
					LinkEnvironment.bPGOProfile
				);
			}

			if (CompileEnvironment.bPGOOptimize)
			{
				DirectoryReference BaseDir = Unreal.EngineDirectory;
				if (Target.ProjectFile != null)
				{
					BaseDir = DirectoryReference.FromFile(Target.ProjectFile);
				}
				CompileEnvironment.PGODirectory = Path.Combine(BaseDir.FullName, "Build", Target.Platform.ToString(), "PGO").Replace('\\', '/') + "/";
				CompileEnvironment.PGOFilenamePrefix = "profile.profdata";

				LinkEnvironment.PGODirectory = CompileEnvironment.PGODirectory;
				LinkEnvironment.PGOFilenamePrefix = CompileEnvironment.PGOFilenamePrefix;
			}

			// For consistency with other platforms, also enable LTO whenever doing profile-guided optimizations.
			// Obviously both PGI (instrumented) and PGO (optimized) binaries need to have that
			if (CompileEnvironment.bPGOProfile || CompileEnvironment.bPGOOptimize)
			{
				CompileEnvironment.bAllowLTCG = true;
				LinkEnvironment.bAllowLTCG = true;
			}

			if (CompileEnvironment.bAllowLTCG != LinkEnvironment.bAllowLTCG)
			{
				throw new BuildException("Inconsistency between LTCG settings in Compile ({0}) and Link ({1}) environments",
					CompileEnvironment.bAllowLTCG,
					LinkEnvironment.bAllowLTCG
				);
			}

			CompileEnvironment.Definitions.Add("INT64_T_TYPES_NOT_LONG_LONG=1");

			if (Target.LinuxPlatform.bEnableRayTracing)
			{
				CompileEnvironment.Definitions.Add("RHI_RAYTRACING=1");
			}

			// link with Linux libraries.
			LinkEnvironment.SystemLibraries.Add("pthread");

			// let this class or a sub class do settings specific to that class
			SetUpSpecificEnvironment(Target, CompileEnvironment, LinkEnvironment);
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
			ClangToolChainOptions Options = ClangToolChainOptions.None;

			if (Target.LinuxPlatform.bEnableAddressSanitizer)
			{
				Options |= ClangToolChainOptions.EnableAddressSanitizer;

				if (Target.LinkType != TargetLinkType.Monolithic)
				{
					Options |= ClangToolChainOptions.EnableSharedSanitizer;
				}
			}
			if (Target.LinuxPlatform.bEnableThreadSanitizer)
			{
				Options |= ClangToolChainOptions.EnableThreadSanitizer;

				if (Target.LinkType != TargetLinkType.Monolithic)
				{
					throw new BuildException("Thread Sanitizer (TSan) unsupported for non-monolithic builds");
				}
			}
			if (Target.LinuxPlatform.bEnableUndefinedBehaviorSanitizer)
			{
				Options |= ClangToolChainOptions.EnableUndefinedBehaviorSanitizer;

				if (Target.LinkType != TargetLinkType.Monolithic)
				{
					Options |= ClangToolChainOptions.EnableSharedSanitizer;
				}
			}
			if (Target.LinuxPlatform.bEnableMemorySanitizer)
			{
				Options |= ClangToolChainOptions.EnableMemorySanitizer;

				if (Target.LinkType != TargetLinkType.Monolithic)
				{
					throw new BuildException("Memory Sanitizer (MSan) unsupported for non-monolithic builds");
				}
			}
			if (Target.bAllowLTCG && Target.bPreferThinLTO)
			{
				Options |= ClangToolChainOptions.EnableThinLTO;
			}

			// When building a monolithic editor we have to avoid using objcopy.exe as it cannot handle files
			// larger then 4GB. This is only an issue with our binutils objcopy.exe.
			// llvm-objcopy.exe does not have this issue and once we switch over to using that in clang 10.0.1 we can remove this!
			if ((BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64) &&
				(Target.LinkType == TargetLinkType.Monolithic) &&
				(Target.Type == TargetType.Editor))
			{
				Options |= ClangToolChainOptions.DisableSplitDebugInfoWithObjCopy;
			}

			if (Target.LinuxPlatform.bTuneDebugInfoForLLDB)
			{
				Options |= ClangToolChainOptions.TuneDebugInfoForLLDB;
			}

			if (Target.LinuxPlatform.bPreservePSYM)
			{
				Options |= ClangToolChainOptions.PreservePSYM;
			}

			// Disable color logging if we are on a build machine
			if (Environment.GetEnvironmentVariable("IsBuildMachine") == "1")
			{
				Log.ColorConsoleOutput = false;
			}

			return new LinuxToolChain(Target.Architecture, SDK, Options, Logger);
		}

		/// <inheritdoc/>
		public override void Deploy(TargetReceipt Receipt)
		{
			new UEDeployLinux(Logger).PrepTargetForDeployment(Receipt);
		}
	}

	class UEDeployLinux: UEBuildDeploy
	{
		public UEDeployLinux(ILogger InLogger)
			: base(InLogger)
		{
		}

		public override bool PrepTargetForDeployment(TargetReceipt Receipt)
		{
			return base.PrepTargetForDeployment(Receipt);
		}
	}

	class LinuxPlatformFactory : UEBuildPlatformFactory
	{
		public override UnrealTargetPlatform TargetPlatform
		{
			get { return UnrealTargetPlatform.Linux; }
		}

		/// <summary>
		/// Register the platform with the UEBuildPlatform class
		/// </summary>
		public override void RegisterBuildPlatforms(ILogger Logger)
		{
			LinuxPlatformSDK SDK = new LinuxPlatformSDK(Logger);
			LinuxPlatformSDK SDKArm64 = new LinuxPlatformSDK(Logger);

			// Register this build platform for Linux x86-64 and Arm64
			UEBuildPlatform.RegisterBuildPlatform(new LinuxPlatform(UnrealTargetPlatform.Linux, SDK, Logger), Logger);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Linux, UnrealPlatformGroup.Linux);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Linux, UnrealPlatformGroup.Unix);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.Linux, UnrealPlatformGroup.Desktop);

			UEBuildPlatform.RegisterBuildPlatform(new LinuxPlatform(UnrealTargetPlatform.LinuxArm64, SDKArm64, Logger), Logger);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.LinuxArm64, UnrealPlatformGroup.Linux);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.LinuxArm64, UnrealPlatformGroup.Unix);
			UEBuildPlatform.RegisterPlatformWithGroup(UnrealTargetPlatform.LinuxArm64, UnrealPlatformGroup.Desktop);
		}
	}
}
