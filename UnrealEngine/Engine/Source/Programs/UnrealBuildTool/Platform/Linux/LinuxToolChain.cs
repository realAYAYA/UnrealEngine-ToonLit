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
	class LinuxToolChain : ClangToolChain
	{
		protected class LinuxToolChainInfo : ClangToolChainInfo
		{
			// cache the location of NDK tools
			public bool bIsCrossCompiling { get; init; }
			public DirectoryReference? BaseLinuxPath { get; init; }
			public DirectoryReference? MultiArchRoot { get; init; }

			public FileReference Objcopy { get; init; }
			public FileReference DumpSyms { get; init; }
			public FileReference BreakpadEncoder { get; init; }

			public LinuxToolChainInfo(DirectoryReference? BaseLinuxPath, DirectoryReference? MultiArchRoot, FileReference Clang, FileReference Archiver, FileReference Objcopy, ILogger Logger)
				: base(Clang, Archiver, Logger)
			{
				this.BaseLinuxPath = BaseLinuxPath;
				this.MultiArchRoot = MultiArchRoot;
				this.Objcopy = Objcopy;
				// these are supplied by the engine and do not change depending on the circumstances
				DumpSyms = FileReference.Combine(Unreal.EngineDirectory, "Binaries", "Linux", $"dump_syms{BuildHostPlatform.Current.BinarySuffix}");
				BreakpadEncoder = FileReference.Combine(Unreal.EngineDirectory, "Binaries", "Linux", $"BreakpadSymbolEncoder{BuildHostPlatform.Current.BinarySuffix}");
			}
		}

		/** Flavor of the current build (target triplet)*/
		string Architecture;

		/** Whether the compiler is set up to produce PIE executables by default */
		bool bSuppressPIE = false;

		/** Pass --gdb-index option to linker to generate .gdb_index section. */
		protected bool bGdbIndexSection = true;

		/** Allows you to override the maximum binary size allowed to be passed to objcopy.exe when cross building on Windows. */
		/** Max value is 2GB, due to bat file limitation */
		protected UInt64 MaxBinarySizeOverrideForObjcopy = 0;

		/** Platform SDK to use */
		protected LinuxPlatformSDK PlatformSDK;

		protected LinuxToolChainInfo LinuxInfo => (Info as LinuxToolChainInfo)!;

		public LinuxToolChain(string InArchitecture, LinuxPlatformSDK InSDK, ClangToolChainOptions InOptions, ILogger InLogger)
			: this(UnrealTargetPlatform.Linux, InArchitecture, InSDK, InOptions, InLogger)
		{
			CheckDefaultCompilerSettings();

			// prevent unknown clangs since the build is likely to fail on too old or too new compilers
			if (CompilerVersionLessThan(13, 0, 0) || CompilerVersionGreaterOrEqual(14, 0, 0))
			{
				throw new BuildException(
					string.Format("This version of the Unreal Engine can only be compiled with clang 13.0. clang {0} may not build it - please use a different version.",
						Info.ClangVersion)
					);
			}
		}

		public LinuxToolChain(UnrealTargetPlatform InPlatform, string InArchitecture, LinuxPlatformSDK InSDK, ClangToolChainOptions InOptions, ILogger InLogger)
			: base(InOptions, InLogger)
		{
			Architecture = InArchitecture;
			PlatformSDK = InSDK;
		}

		protected override ClangToolChainInfo GetToolChainInfo()
		{
			DirectoryReference? MultiArchRoot = PlatformSDK.GetSDKLocation();
			DirectoryReference? BaseLinuxPath = PlatformSDK.GetBaseLinuxPathForArchitecture(Architecture);

			bool bForceUseSystemCompiler = PlatformSDK.ForceUseSystemCompiler();

			if (bForceUseSystemCompiler)
			{
				// use native linux toolchain
				FileReference? ClangPath = FileReference.FromString(LinuxCommon.WhichClang(Logger));
				FileReference? LlvmArPath = FileReference.FromString(LinuxCommon.Which("llvm-ar", Logger));
				FileReference? ObjcopyPath = FileReference.FromString(LinuxCommon.Which("llvm-objcopy", Logger));

				if (ClangPath == null)
				{
					throw new BuildException("Unable to find system clang; cannot instantiate Linux toolchain");
				}
				else if (LlvmArPath == null)
				{
					throw new BuildException("Unable to find system llvm-ar; cannot instantiate Linux toolchain");
				}
				else if (ObjcopyPath == null)
				{
					throw new BuildException("Unable to find system llvm-objcopy; cannot instantiate Linux toolchain");
				}

				// When compiling on Linux, use a faster way to relink circularly dependent libraries.
				// Race condition between actions linking to the .so and action overwriting it is avoided thanks to inodes
				bUseFixdeps = false;

				bIsCrossCompiling = false;

				return new LinuxToolChainInfo(null, null, ClangPath, LlvmArPath, ObjcopyPath, Logger);
			}
			else
			{
				if (BaseLinuxPath == null)
				{
					throw new BuildException("LINUX_MULTIARCH_ROOT environment variable is not set; cannot instantiate Linux toolchain");
				}
				if (MultiArchRoot == null)
				{
					MultiArchRoot = BaseLinuxPath;
					Logger.LogInformation("Using LINUX_ROOT (deprecated, consider LINUX_MULTIARCH_ROOT)");
				}

				// set up the path to our toolchain
				FileReference ClangPath = FileReference.Combine(BaseLinuxPath, "bin", $"clang++{BuildHostPlatform.Current.BinarySuffix}");
				FileReference LlvmArPath = FileReference.Combine(BaseLinuxPath, "bin", $"llvm-ar{BuildHostPlatform.Current.BinarySuffix}");
				FileReference ObjcopyPath = FileReference.Combine(BaseLinuxPath, "bin", $"llvm-objcopy{BuildHostPlatform.Current.BinarySuffix}");

				// When cross-compiling on Windows, use old FixDeps. It is slow, but it does not have timing issues
				bUseFixdeps = BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64;

				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
				{
					Environment.SetEnvironmentVariable("LC_ALL", "C");
				}

				bIsCrossCompiling = true;

				return new LinuxToolChainInfo(BaseLinuxPath, MultiArchRoot, ClangPath, LlvmArPath, ObjcopyPath, Logger);
			}
		}

		protected virtual bool CrossCompiling()
		{
			return bIsCrossCompiling;
		}

		protected internal virtual string GetDumpEncodeDebugCommand(LinkEnvironment LinkEnvironment, FileItem OutputFile)
		{
			bool bUseCmdExe = BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64;
			string DumpCommand = bUseCmdExe ? "\"{0}\" \"{1}\" \"{2}\" 2>NUL" : "\"{0}\" -c -o \"{2}\" \"{1}\"";
			FileItem EncodedBinarySymbolsFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.OutputDirectory!.FullName, OutputFile.Location.GetFileNameWithoutExtension() + ".sym"));
			FileItem SymbolsFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.LocalShadowDirectory!.FullName, OutputFile.Location.GetFileName() + ".psym"));
			FileItem StrippedFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, OutputFile.Location.GetFileName() + "_nodebug"));
			FileItem DebugFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.OutputDirectory.FullName, OutputFile.Location.GetFileNameWithoutExtension() + ".debug"));

			if (Options.HasFlag(ClangToolChainOptions.PreservePSYM))
			{
				SymbolsFile = FileItem.GetItemByPath(Path.Combine(LinkEnvironment.OutputDirectory.FullName, OutputFile.Location.GetFileNameWithoutExtension() + ".psym"));
			}

			StringWriter Out = new StringWriter();
			Out.NewLine = bUseCmdExe ? "\r\n" : "\n";

			// dump_syms
			Out.WriteLine(DumpCommand,
				LinuxInfo.DumpSyms,
				OutputFile.AbsolutePath,
				SymbolsFile.AbsolutePath
			);

			// encode breakpad symbols
			Out.WriteLine("\"{0}\" \"{1}\" \"{2}\"",
				LinuxInfo.BreakpadEncoder,
				SymbolsFile.AbsolutePath,
				EncodedBinarySymbolsFile.AbsolutePath
			);

			if (!Options.HasFlag(ClangToolChainOptions.DisableSplitDebugInfoWithObjCopy) && LinkEnvironment.bCreateDebugInfo)
			{
				if (MaxBinarySizeOverrideForObjcopy > 0 && bUseCmdExe)
				{
					Out.WriteLine("for /F \"tokens=*\" %%F in (\"{0}\") DO set size=%%~zF",
						OutputFile.AbsolutePath
					);

					Out.WriteLine("if %size% LSS {0} (", MaxBinarySizeOverrideForObjcopy);
				}

				// objcopy stripped file
				Out.WriteLine("\"{0}\" --strip-all \"{1}\" \"{2}\"",
					LinuxInfo.Objcopy,
					OutputFile.AbsolutePath,
					StrippedFile.AbsolutePath
				);

				// objcopy debug file
				Out.WriteLine("\"{0}\" --only-keep-debug \"{1}\" \"{2}\"",
					LinuxInfo.Objcopy,
					OutputFile.AbsolutePath,
					DebugFile.AbsolutePath
				);

				// objcopy link debug file to final so
				Out.WriteLine("\"{0}\" --add-gnu-debuglink=\"{1}\" \"{2}\" \"{3}.temp\"",
					LinuxInfo.Objcopy,
					DebugFile.AbsolutePath,
					StrippedFile.AbsolutePath,
					OutputFile.AbsolutePath
				);

				if (bUseCmdExe)
				{
					// Only move the temp final elf file once its done being linked by objcopy
					Out.WriteLine("move /Y \"{0}.temp\" \"{1}\"",
						OutputFile.AbsolutePath,
						OutputFile.AbsolutePath
					);

					if (MaxBinarySizeOverrideForObjcopy > 0)
					{
						// If we have an override size, then we need to create a dummy file if that size is exceeded
						Out.WriteLine(") ELSE (");
						Out.WriteLine("echo DummyDebug >> \"{0}\"", DebugFile.AbsolutePath);
						Out.WriteLine(")");
					}
				}
				else
				{
					// Only move the temp final elf file once its done being linked by objcopy
					Out.WriteLine("mv \"{0}.temp\" \"{1}\"",
						OutputFile.AbsolutePath,
						OutputFile.AbsolutePath
					);

					// Change the debug file to normal permissions. It was taking on the +x rights from the output file
					Out.WriteLine("chmod 644 \"{0}\"",
						DebugFile.AbsolutePath
					);
				}
			}
			else
			{
				// If we have disabled objcopy then we need to create a dummy debug file
				Out.WriteLine("echo DummyDebug >> \"{0}\"",
					DebugFile.AbsolutePath
				);
			}

			return Out.ToString();
		}

		/// <summary>
		/// Checks default compiler settings
		/// </summary>
		private void CheckDefaultCompilerSettings()
		{
			using (Process Proc = new Process())
			{
				Proc.StartInfo.UseShellExecute = false;
				Proc.StartInfo.CreateNoWindow = true;
				Proc.StartInfo.RedirectStandardOutput = true;
				Proc.StartInfo.RedirectStandardError = true;
				Proc.StartInfo.RedirectStandardInput = true;

				if (FileReference.Exists(Info.Clang))
				{
					Proc.StartInfo.FileName = Info.Clang.FullName;
					Proc.StartInfo.Arguments = " -E -dM -";

					Proc.Start();
					Proc.StandardInput.Close();

					for (; ; )
					{
						string? CompilerDefine = Proc.StandardOutput.ReadLine();
						if (string.IsNullOrEmpty(CompilerDefine))
						{
							Proc.WaitForExit();
							break;
						}

						if (CompilerDefine.Contains("__PIE__") || CompilerDefine.Contains("__pie__"))
						{
							bSuppressPIE = true;
						}
					}
				}
				else
				{
					// other compilers aren't implemented atm
				}
			}
		}

		/// <summary>
		/// Architecture-specific compiler switches
		/// </summary>
		static string ArchitectureSpecificSwitches(string Architecture)
		{
			string Result = "";

			if (Architecture.StartsWith("arm") || Architecture.StartsWith("aarch64"))
			{
				Result += "-fsigned-char";
			}

			return Result;
		}

		static string ArchitectureSpecificDefines(string Architecture)
		{
			string Result = "";

			if (Architecture.StartsWith("x86_64") || Architecture.StartsWith("aarch64"))
			{
				Result += "-D_LINUX64";
			}

			return Result;
		}

		private static bool ShouldUseLibcxx(string Architecture)
		{
			// set UE_LINUX_USE_LIBCXX to either 0 or 1. If unset, defaults to 1.
			string? UseLibcxxEnvVarOverride = Environment.GetEnvironmentVariable("UE_LINUX_USE_LIBCXX");
			if (string.IsNullOrEmpty(UseLibcxxEnvVarOverride) || UseLibcxxEnvVarOverride == "1")
			{
				// at the moment ARM32 libc++ remains missing
				return Architecture.StartsWith("x86_64") || Architecture.StartsWith("aarch64");
			}
			return false;
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_WarningsAndErrors(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_WarningsAndErrors(CompileEnvironment, Arguments);

			//Arguments.Add("-Wunreachable-code");            // additional warning not normally included in Wall: warns if there is code that will never be executed - not helpful due to bIsGCC and similar

			Arguments.Add("-Wno-undefined-bool-conversion"); // hides checking if 'this' pointer is null
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Optimizations(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Optimizations(CompileEnvironment, Arguments);

			// Unlike on other platforms, allow LTO be specified independently of PGO
			if (CompileEnvironment.bAllowLTCG)
			{
				if ((Options & ClangToolChainOptions.EnableThinLTO) != 0)
				{
					Arguments.Add("-flto=thin");
				}
				else
				{
					Arguments.Add("-flto");
				}
			}

			// optimization level
			if (!CompileEnvironment.bOptimizeCode)
			{
				Arguments.Add("-O0");
			}
			else
			{
				// Don't over optimise if using Address/MemorySanitizer or you'll get false positive errors due to erroneous optimisation of necessary Address/MemorySanitizer instrumentation.
				if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) || Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
				{
					Arguments.Add("-O1 -g");

					// This enables __asan_default_options() in UnixCommonStartup.h which disables the leak detector
					Arguments.Add("-DDISABLE_ASAN_LEAK_DETECTOR=1");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
				{
					Arguments.Add("-O1 -g");
				}
				else
				{
					if (CompileEnvironment.OptimizationLevel == OptimizationMode.Size)
					{
						Arguments.Add("-Oz");
					}
					else if (CompileEnvironment.OptimizationLevel == OptimizationMode.SizeAndSpeed)
					{
						Arguments.Add("-Os");
						if (Architecture.StartsWith("aarch64"))
						{
							Arguments.Add("-moutline");
						}
					}
					else
					{
						Arguments.Add("-O3");
					}
				}
			}

			bool bRetainFramePointers = CompileEnvironment.bRetainFramePointers
				|| Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) || Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer)
				|| CompileEnvironment.Configuration == CppConfiguration.Debug;

			if (CompileEnvironment.Configuration == CppConfiguration.Shipping)
			{
				if (!bRetainFramePointers)
				{
					Arguments.Add("-fomit-frame-pointer");
				}
			}
			// switches to help debugging
			else if (CompileEnvironment.Configuration == CppConfiguration.Debug)
			{
				Arguments.Add("-fno-inline");                   // disable inlining for better debuggability (e.g. callstacks, "skip file" in gdb)
				Arguments.Add("-fstack-protector");             // detect stack smashing
			}

			if (bRetainFramePointers)
			{
				Arguments.Add("-fno-optimize-sibling-calls");
				Arguments.Add("-fno-omit-frame-pointer");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Debugging(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Debugging(CompileEnvironment, Arguments);

			// debug info
			// bCreateDebugInfo is normally set for all configurations, including Shipping - this is needed to enable callstack in Shipping builds (proper resolution: UEPLAT-205, separate files with debug info)
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Arguments.Add("-gdwarf-4");

				if (bGdbIndexSection)
				{
					// Generate .debug_pubnames and .debug_pubtypes sections in a format suitable for conversion into a
					// GDB index. This option is only useful with a linker that can produce GDB index version 7.
					Arguments.Add("-ggnu-pubnames");
				}

				if (Options.HasFlag(ClangToolChainOptions.TuneDebugInfoForLLDB))
				{
					Arguments.Add("-glldb");
				}
			}

			if (CompileEnvironment.bHideSymbolsByDefault)
			{
				Arguments.Add("-fvisibility-ms-compat");
				Arguments.Add("-fvisibility-inlines-hidden");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompilerArguments_Sanitizers(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompilerArguments_Sanitizers(CompileEnvironment, Arguments);

			// ASan
			if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer))
			{
				// Force using the ANSI allocator if ASan is enabled
				Arguments.Add("-DFORCE_ANSI_ALLOCATOR=1");
			}

			// TSan
			if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
			{
				// Force using the ANSI allocator if TSan is enabled
				Arguments.Add("-DFORCE_ANSI_ALLOCATOR=1");
			}

			// UBSan
			if (Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer))
			{
				Arguments.Add("-fno-sanitize=vptr");
			}

			// MSan
			if (Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
			{
				// Force using the ANSI allocator if MSan is enabled
				// -fsanitize-memory-track-origins adds a 1.5x-2.5x slow down ontop of MSan normal amount of overhead
				// -fsanitize-memory-track-origins=1 is faster but collects only allocation points but not intermediate stores
				Arguments.Add("-fsanitize-memory-track-origins");
				Arguments.Add("-DFORCE_ANSI_ALLOCATOR=1");
			}
		}

		/// <inheritdoc/>
		protected override void GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Global(CompileEnvironment, Arguments);

			// build up the commandline common to C and C++

			if (ShouldUseLibcxx(CompileEnvironment.Architecture))
			{
				Arguments.Add("-nostdinc++");
				Arguments.Add(GetSystemIncludePathArgument(DirectoryReference.Combine(Unreal.EngineSourceDirectory, "ThirdParty", "Unix", "LibCxx", "include")));
				Arguments.Add(GetSystemIncludePathArgument(DirectoryReference.Combine(Unreal.EngineSourceDirectory, "ThirdParty", "Unix", "LibCxx", "include", "c++", "v1")));
			}

			if (CompilerVersionGreaterOrEqual(12, 0, 0))
			{
				Arguments.Add("-fbinutils-version=2.36");
			}

			if (!CompileEnvironment.Architecture.StartsWith("x86_64"))
			{
				Arguments.Add("-funwind-tables");               // generate unwind tables as they are needed for backtrace (on x86(64) they are generated implicitly)
			}

			Arguments.Add(ArchitectureSpecificSwitches(CompileEnvironment.Architecture));

			Arguments.Add("-fno-math-errno");               // do not assume that math ops have side effects

			Arguments.Add(GetRTTIFlag(CompileEnvironment)); // flag for run-time type info

			if (CompileEnvironment.Architecture.StartsWith("x86_64"))
			{
				Arguments.Add("-mssse3"); // enable ssse3 by default for x86. This is default on for MSVC so lets reflect that here
			}

			//Arguments.Add("-DOPERATOR_NEW_INLINE=FORCENOINLINE");

			if (CompileEnvironment.bIsBuildingDLL)
			{
				Arguments.Add("-fPIC");
				// Use local-dynamic TLS model. This generates less efficient runtime code for __thread variables, but avoids problems of running into
				// glibc/ld.so limit (DTV_SURPLUS) for number of dlopen()'ed DSOs with static TLS (see e.g. https://www.cygwin.com/ml/libc-help/2013-11/msg00033.html)
				Arguments.Add("-ftls-model=local-dynamic");
			}
			else
			{
				Arguments.Add("-ffunction-sections");
				Arguments.Add("-fdata-sections");
			}

			if (bSuppressPIE && !CompileEnvironment.bIsBuildingDLL)
			{
				Arguments.Add("-fno-PIE");
			}

			if (PlatformSDK.bVerboseCompiler)
			{
				Arguments.Add("-v");                            // for better error diagnosis
			}

			Arguments.Add(ArchitectureSpecificDefines(CompileEnvironment.Architecture));
			if (CrossCompiling())
			{
				if (!String.IsNullOrEmpty(CompileEnvironment.Architecture))
				{
					Arguments.Add($"-target {CompileEnvironment.Architecture}");        // Set target triple
				}
				Arguments.Add($"--sysroot=\"{NormalizeCommandLinePath(LinuxInfo.BaseLinuxPath!)}\"");
			}
		}

		/// <inheritdoc/>
		protected override string EscapePreprocessorDefinition(string Definition)
		{
			string[] SplitData = Definition.Split('=');
			string? Key = SplitData.ElementAtOrDefault(0);
			string? Value = SplitData.ElementAtOrDefault(1);

			if (string.IsNullOrEmpty(Key)) { return ""; }
			if (!string.IsNullOrEmpty(Value))
			{
				if (!Value.StartsWith("\"") && (Value.Contains(" ") || Value.Contains("$")))
				{
					Value = Value.Trim('\"');       // trim any leading or trailing quotes
					Value = "\"" + Value + "\"";    // ensure wrap string with double quotes
				}

				// replace double quotes to escaped double quotes if exists
				Value = Value.Replace("\"", "\\\"");
			}

			return Value == null
				? string.Format("{0}", Key)
				: string.Format("{0}={1}", Key, Value);
		}

		protected virtual void GetLinkArguments(LinkEnvironment LinkEnvironment, List<string> Arguments)
		{
			Arguments.Add((BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64) ? "-fuse-ld=lld.exe" : "-fuse-ld=lld");

			// debugging symbols
			// Applying to all configurations @FIXME: temporary hack for FN to enable callstack in Shipping builds (proper resolution: UEPLAT-205)
			Arguments.Add("-rdynamic");   // needed for backtrace_symbols()...

			if (LinkEnvironment.bIsBuildingDLL)
			{
				Arguments.Add("-shared");
			}
			else
			{
				// ignore unresolved symbols in shared libs
				Arguments.Add("-Wl,--unresolved-symbols=ignore-in-shared-libs");
			}

			if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
			{
				Arguments.Add("-g");

				if (Options.HasFlag(ClangToolChainOptions.EnableSharedSanitizer))
				{
					Arguments.Add("-shared-libsan");
				}

				if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer))
				{
					Arguments.Add("-fsanitize=address");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
				{
					Arguments.Add("-fsanitize=thread");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer))
				{
					Arguments.Add("-fsanitize=undefined");
				}
				else if (Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
				{
					// -fsanitize-memory-track-origins adds a 1.5x-2.5x slow ontop of MSan normal amount of overhead
					// -fsanitize-memory-track-origins=1 is faster but collects only allocation points but not intermediate stores
					Arguments.Add("-fsanitize=memory -fsanitize-memory-track-origins");
				}

				if (CrossCompiling())
				{
					Arguments.Add(string.Format("-Wl,-rpath=\"{0}/lib/clang/{1}.{2}.{3}/lib/linux\"",
							LinuxInfo.BaseLinuxPath, Info.ClangVersion.Major, Info.ClangVersion.Minor, Info.ClangVersion.Build));
				}
			}

			if (LinkEnvironment.bCreateDebugInfo && bGdbIndexSection)
			{
				// Generate .gdb_index section. On my machine, this cuts symbol loading time (breaking at main) from 45
				// seconds to 17 seconds (with gdb v8.3.1).
				Arguments.Add("-Wl,--gdb-index");
			}

			// RPATH for third party libs
			Arguments.Add("-Wl,-rpath=${ORIGIN}");
			Arguments.Add("-Wl,-rpath-link=${ORIGIN}");
			Arguments.Add("-Wl,-rpath=${ORIGIN}/..");   // for modules that are in sub-folders of the main Engine/Binary/Linux folder
			if (LinkEnvironment.Architecture.StartsWith("x86_64"))
			{
				Arguments.Add("-Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/Qualcomm/Linux");
			}
			else
			{
				// x86_64 is now using updated ICU that doesn't need extra .so
				Arguments.Add("-Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/ICU/icu4c-53_1/Unix/" + LinkEnvironment.Architecture);
			}

			Arguments.Add("-Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/OpenVR/OpenVRv1_5_17/linux64");

			// @FIXME: Workaround for generating RPATHs for launching on devices UE-54136
			Arguments.Add("-Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/PhysX3/Unix/x86_64-unknown-linux-gnu");
			Arguments.Add("-Wl,-rpath=${ORIGIN}/../../../Engine/Binaries/ThirdParty/Intel/Embree/Embree2140/Linux/x86_64-unknown-linux-gnu/lib");

			// Some OS ship ld with new ELF dynamic tags, which use DT_RUNPATH vs DT_RPATH. Since DT_RUNPATH do not propagate to dlopen()ed DSOs,
			// this breaks the editor on such systems. See https://kenai.com/projects/maxine/lists/users/archive/2011-01/message/12 for details
			Arguments.Add("-Wl,--disable-new-dtags");

			// This severely improves runtime linker performance. Without using FixDeps the impact on link time is not as big.
			Arguments.Add("-Wl,--as-needed");

			// Additionally speeds up editor startup by 1-2s
			Arguments.Add("-Wl,--hash-style=gnu");

			// This apparently can help LLDB speed up symbol lookups
			Arguments.Add("-Wl,--build-id");
			if (!LinkEnvironment.bIsBuildingDLL)
			{
				Arguments.Add("-Wl,--gc-sections");

				if (bSuppressPIE)
				{
					Arguments.Add("-Wl,-no-pie");
				}
			}

			// Profile Guided Optimization (PGO) and Link Time Optimization (LTO)
			// Whether we actually can enable that is checked in CanUseAdvancedLinkerFeatures() earlier
			if (LinkEnvironment.bPGOOptimize)
			{
				//
				// Clang emits a warning for each compiled function that doesn't have a matching entry in the profile data.
				// This can happen when the profile data is older than the binaries we're compiling.
				//
				// Disable this warning. It's far too verbose.
				//
				Arguments.Add("-Wno-backend-plugin");

				Log.TraceInformationOnce("Enabling Profile Guided Optimization (PGO). Linking will take a while.");
				Arguments.Add(string.Format("-fprofile-instr-use=\"{0}\"", Path.Combine(LinkEnvironment.PGODirectory!, LinkEnvironment.PGOFilenamePrefix!)));
			}
			else if (LinkEnvironment.bPGOProfile)
			{
				Log.TraceInformationOnce("Enabling Profile Guided Instrumentation (PGI). Linking will take a while.");
				Arguments.Add("-fprofile-generate");
			}

			// whether we actually can do that is checked in CanUseAdvancedLinkerFeatures() earlier
			if (LinkEnvironment.bAllowLTCG)
			{
				if ((Options & ClangToolChainOptions.EnableThinLTO) != 0)
				{
					Arguments.Add(String.Format(" -flto=thin -Wl,--thinlto-jobs={0}", Utils.GetPhysicalProcessorCount()));
				}
				else
				{
					Arguments.Add("-flto");
				}
			}

			if (CrossCompiling())
			{
				Arguments.Add($"-target {LinkEnvironment.Architecture}");        // Set target triple
				DirectoryReference SysRootPath = LinuxInfo.BaseLinuxPath!;
				Arguments.Add($"--sysroot=\"{NormalizeCommandLinePath(SysRootPath)}\"");

				// Linking with the toolchain on linux appears to not search usr/
				if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux)
				{
					Arguments.Add($"-B\"{NormalizeCommandLinePath(DirectoryReference.Combine(SysRootPath, "usr", "lib"))}\"");
					Arguments.Add($"-B\"{NormalizeCommandLinePath(DirectoryReference.Combine(SysRootPath, "usr", "lib64"))}\"");
					Arguments.Add($"-L\"{NormalizeCommandLinePath(DirectoryReference.Combine(SysRootPath, "usr", "lib"))}\"");
					Arguments.Add($"-L\"{NormalizeCommandLinePath(DirectoryReference.Combine(SysRootPath, "usr", "lib64"))}\"");
				}
			}
		}

		string GetArchiveArguments(LinkEnvironment LinkEnvironment)
		{
			return " rcs";
		}

		// cache the location of NDK tools
		protected bool bIsCrossCompiling;
		/// <summary>
		/// Whether to use old, slower way to relink circularly dependent libraries.
		/// It makes sense to use it when cross-compiling on Windows due to race conditions between actions reading and modifying the libs.
		/// </summary>
		private bool bUseFixdeps = false;

		/// <summary>
		/// Track which scripts need to be deleted before appending to
		/// </summary>
		private bool bHasWipedFixDepsScript = false;

		/// <summary>
		/// Holds all the binaries for a particular target (except maybe the executable itself).
		/// </summary>
		private static List<FileItem> AllBinaries = new List<FileItem>();

		/// <summary>
		/// Tracks that information about used C++ library is only printed once
		/// </summary>
		private bool bHasPrintedBuildDetails = false;
		protected void PrintBuildDetails(CppCompileEnvironment CompileEnvironment, ILogger Logger)
		{
			Logger.LogInformation("------- Build details --------");
			Logger.LogInformation("Using {ToolchainInfo}.", LinuxInfo.BaseLinuxPath == null ? "system toolchain" : $"toolchain located at '{LinuxInfo.BaseLinuxPath}'");
			Logger.LogInformation("Using clang ({ClangPath}) version '{ClangVersionString}' (string), {ClangVersionMajor} (major), {ClangVersionMinor} (minor), {ClangVersionPatch} (patch)",
				Info.Clang, Info.ClangVersionString, Info.ClangVersion.Major, Info.ClangVersion.Minor, Info.ClangVersion.Build);

			// inform the user which C++ library the engine is going to be compiled against - important for compatibility with third party code that uses STL
			Logger.LogInformation("Using {Lib} standard C++ library.", ShouldUseLibcxx(CompileEnvironment.Architecture) ? "bundled libc++" : "compiler default (most likely libstdc++)");
			Logger.LogInformation("Using lld linker");
			Logger.LogInformation("Using llvm-ar ({LlvmAr}) version '{LlvmArVersionString} (string)'", Info.Archiver, Info.ArchiverVersionString);

			if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer) ||
				Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
			{
				string SanitizerInfo = "Building with:";
				string StaticOrShared = Options.HasFlag(ClangToolChainOptions.EnableSharedSanitizer) ? " dynamically" : " statically";

				SanitizerInfo += Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer) ? StaticOrShared + " linked AddressSanitizer" : "";
				SanitizerInfo += Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer) ? StaticOrShared + " linked ThreadSanitizer" : "";
				SanitizerInfo += Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer) ? StaticOrShared + " linked UndefinedBehaviorSanitizer" : "";
				SanitizerInfo += Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer) ? StaticOrShared + " linked MemorySanitizer" : "";

				Logger.LogInformation("{SanitizerInfo}", SanitizerInfo);
			}

			// Also print other once-per-build information
			if (bUseFixdeps)
			{
				Logger.LogInformation("Using old way to relink circularly dependent libraries (with a FixDeps step).");
			}
			else
			{
				Logger.LogInformation("Using fast way to relink  circularly dependent libraries (no FixDeps).");
			}

			if (CompileEnvironment.bPGOOptimize)
			{
				Logger.LogInformation("Using PGO (profile guided optimization).");
				Logger.LogInformation("  Directory for PGO data files='{CompileEnvironmentPGODirectory}'", CompileEnvironment.PGODirectory);
				Logger.LogInformation("  Prefix for PGO data files='{CompileEnvironmentPGOFilenamePrefix}'", CompileEnvironment.PGOFilenamePrefix);
			}

			if (CompileEnvironment.bPGOProfile)
			{
				Logger.LogInformation("Using PGI (profile guided instrumentation).");
			}

			if (CompileEnvironment.bAllowLTCG)
			{
				Logger.LogInformation("Using LTO (link-time optimization).");
			}

			if (bSuppressPIE)
			{
				Logger.LogInformation("Compiler is set up to generate position independent executables by default, but we're suppressing it.");
			}
			Logger.LogInformation("------------------------------");
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph)
		{
			List<string> GlobalArguments = new();
			GetCompileArguments_Global(CompileEnvironment, GlobalArguments);

			//var BuildPlatform = UEBuildPlatform.GetBuildPlatform(CompileEnvironment.Platform);

			if (!bHasPrintedBuildDetails)
			{
				PrintBuildDetails(CompileEnvironment, Logger);

				bHasPrintedBuildDetails = true;
			}

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in InputFiles)
			{
				CompileCPPFile(CompileEnvironment, SourceFile, OutputDir, ModuleName, Graph, GlobalArguments, Result);
			}

			return Result;
		}

		/// <summary>
		/// Creates an action to archive all the .o files into single .a file
		/// </summary>
		public FileItem CreateArchiveAndIndex(LinkEnvironment LinkEnvironment, IActionGraphBuilder Graph, ILogger Logger)
		{
			// Create an archive action
			Action ArchiveAction = Graph.CreateAction(ActionType.Link);
			ArchiveAction.WorkingDirectory = Unreal.EngineSourceDirectory;
			ArchiveAction.CommandPath = Info.Archiver;

			// this will produce a final library
			ArchiveAction.bProducesImportLibrary = true;

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
			ArchiveAction.ProducedItems.Add(OutputFile);
			ArchiveAction.CommandDescription = "Archive";
			ArchiveAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);
			ArchiveAction.CommandArguments += string.Format("{1} \"{2}\"", GetArchiveArguments(LinkEnvironment), OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				string InputAbsolutePath = InputFile.AbsolutePath.Replace("\\", "/");
				InputFileNames.Add(string.Format("\"{0}\"", InputAbsolutePath));
				ArchiveAction.PrerequisiteItems.Add(InputFile);
			}

			// this won't stomp linker's response (which is not used when compiling static libraries)
			FileReference ResponsePath = GetResponseFileName(LinkEnvironment, OutputFile);
			if (!ProjectFileGenerator.bGenerateProjectFiles)
			{
				FileItem ResponseFileItem = Graph.CreateIntermediateTextFile(ResponsePath, InputFileNames);
				ArchiveAction.PrerequisiteItems.Add(ResponseFileItem);
			}
			ArchiveAction.CommandArguments += string.Format(" @\"{0}\"", ResponsePath.FullName);

			// Add the additional arguments specified by the environment.
			ArchiveAction.CommandArguments += LinkEnvironment.AdditionalArguments;
			ArchiveAction.CommandArguments = ArchiveAction.CommandArguments.Replace("\\", "/");

			if (BuildHostPlatform.Current.ShellType == ShellType.Sh)
			{
				ArchiveAction.CommandArguments += "'";
			}
			else
			{
				ArchiveAction.CommandArguments += "\"";
			}

			// Only execute linking on the local PC.
			ArchiveAction.bCanExecuteRemotely = false;

			return OutputFile;
		}

		public FileItem? FixDependencies(LinkEnvironment LinkEnvironment, FileItem Executable, IActionGraphBuilder Graph, ILogger Logger)
		{
			if (bUseFixdeps)
			{
				Logger.LogDebug("Adding postlink step");

				bool bUseCmdExe = BuildHostPlatform.Current.ShellType == ShellType.Cmd;
				FileReference ShellBinary = BuildHostPlatform.Current.Shell;
				string ExecuteSwitch = bUseCmdExe ? " /C" : ""; // avoid -c so scripts don't need +x
				string ScriptName = bUseCmdExe ? "FixDependencies.bat" : "FixDependencies.sh";

				FileItem FixDepsScript = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.LocalShadowDirectory!, ScriptName));

				// if we never generated one we did not have a circular depends that needed fixing up
				if (!FixDepsScript.Exists)
				{
					return null;
				}

				Action PostLinkAction = Graph.CreateAction(ActionType.Link);
				PostLinkAction.WorkingDirectory = Unreal.EngineSourceDirectory;
				PostLinkAction.CommandPath = ShellBinary;
				PostLinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(Executable.AbsolutePath));
				PostLinkAction.CommandDescription = "FixDeps";
				PostLinkAction.bCanExecuteRemotely = false;
				PostLinkAction.CommandArguments = ExecuteSwitch;

				PostLinkAction.CommandArguments += bUseCmdExe ? " \"" : " -c '";

				FileItem OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(LinkEnvironment.LocalShadowDirectory!, Path.GetFileNameWithoutExtension(Executable.AbsolutePath) + ".link"));

				// Make sure we don't run this script until the all executables and shared libraries
				// have been built.
				PostLinkAction.PrerequisiteItems.Add(Executable);
				foreach (FileItem Dependency in AllBinaries)
				{
					PostLinkAction.PrerequisiteItems.Add(Dependency);
				}

				PostLinkAction.CommandArguments += ShellBinary + ExecuteSwitch + " \"" + FixDepsScript.AbsolutePath + "\" && ";

				// output file should not be empty or it will be rebuilt next time
				string Touch = bUseCmdExe ? "echo \"Dummy\" >> \"{0}\" && copy /b \"{0}\" +,," : "echo \"Dummy\" >> \"{0}\"";

				PostLinkAction.CommandArguments += String.Format(Touch, OutputFile.AbsolutePath);
				PostLinkAction.CommandArguments += bUseCmdExe ? "\"" : "'";

				System.Console.WriteLine("{0} {1}", PostLinkAction.CommandPath, PostLinkAction.CommandArguments);

				PostLinkAction.ProducedItems.Add(OutputFile);
				return OutputFile;
			}
			else
			{
				return null;
			}
		}

		// allow sub-platforms to modify the name of the output file
		protected virtual FileItem GetLinkOutputFile(LinkEnvironment LinkEnvironment)
		{
			return FileItem.GetItemByFileReference(LinkEnvironment.OutputFilePath);
		}


		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			Debug.Assert(!bBuildImportLibraryOnly);

			List<string> RPaths = new List<string>();

			if (LinkEnvironment.bIsBuildingLibrary || bBuildImportLibraryOnly)
			{
				return CreateArchiveAndIndex(LinkEnvironment, Graph, Logger);
			}

			// Create an action that invokes the linker.
			Action LinkAction = Graph.CreateAction(ActionType.Link);
			LinkAction.WorkingDirectory = Unreal.EngineSourceDirectory;

			string LinkCommandString;
			LinkCommandString = "\"" + Info.Clang + "\"";

			// Get link arguments.
			List<string> LinkArguments = new List<string>();
			GetLinkArguments(LinkEnvironment, LinkArguments);
			LinkCommandString += " " + string.Join(' ', LinkArguments);

			// Tell the action that we're building an import library here and it should conditionally be
			// ignored as a prerequisite for other actions
			LinkAction.bProducesImportLibrary = LinkEnvironment.bIsBuildingDLL;

			// Add the output file as a production of the link action.
			FileItem OutputFile = GetLinkOutputFile(LinkEnvironment);
			LinkAction.ProducedItems.Add(OutputFile);
			// LTO/PGO can take a lot of time, make it clear for the user
			if (LinkEnvironment.bPGOProfile)
			{
				LinkAction.CommandDescription = "Link-PGI";
			}
			else if (LinkEnvironment.bPGOOptimize)
			{
				LinkAction.CommandDescription = "Link-PGO";
			}
			else if (LinkEnvironment.bAllowLTCG)
			{
				LinkAction.CommandDescription = "Link-LTO";
			}
			else
			{
				LinkAction.CommandDescription = "Link";
			}
			// because the logic choosing between lld and ld is somewhat messy atm (lld fails to link .DSO due to bugs), make the name of the linker clear
			LinkAction.CommandDescription += (LinkCommandString.Contains("-fuse-ld=lld")) ? " (lld)" : " (ld)";
			LinkAction.CommandVersion = Info.ClangVersionString;
			LinkAction.StatusDescription = Path.GetFileName(OutputFile.AbsolutePath);

			// Add the output file to the command-line.
			LinkCommandString += string.Format(" -o \"{0}\"", OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> ResponseLines = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				ResponseLines.Add(string.Format("\"{0}\"", InputFile.AbsolutePath.Replace("\\", "/")));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			if (LinkEnvironment.bIsBuildingDLL)
			{
				ResponseLines.Add(string.Format(" -soname=\"{0}\"", OutputFile.Location.GetFileName()));
			}

			// Start with the configured LibraryPaths and also add paths to any libraries that
			// we depend on (libraries that we've build ourselves).
			List<DirectoryReference> AllLibraryPaths = LinkEnvironment.SystemLibraryPaths;

			IEnumerable<string> AdditionalLibraries = Enumerable.Concat(LinkEnvironment.SystemLibraries, LinkEnvironment.Libraries.Select(x => x.FullName));
			foreach (string AdditionalLibrary in AdditionalLibraries)
			{
				string PathToLib = Path.GetDirectoryName(AdditionalLibrary)!;
				if (!String.IsNullOrEmpty(PathToLib))
				{
					// make path absolute, because FixDependencies script may be executed in a different directory
					DirectoryReference AbsolutePathToLib = new DirectoryReference(PathToLib);
					if (!AllLibraryPaths.Contains(AbsolutePathToLib))
					{
						AllLibraryPaths.Add(AbsolutePathToLib);
					}
				}

				if ((AdditionalLibrary.Contains("Plugins") || AdditionalLibrary.Contains("Binaries/ThirdParty") || AdditionalLibrary.Contains("Binaries\\ThirdParty")) && Path.GetDirectoryName(AdditionalLibrary) != Path.GetDirectoryName(OutputFile.AbsolutePath))
				{
					string RelativePath = new FileReference(AdditionalLibrary).Directory.MakeRelativeTo(OutputFile.Location.Directory);

					if (LinkEnvironment.bIsBuildingDLL)
					{
						// Remove the root Unreal.RootDirectory from the RuntimeLibaryPath
						string AdditionalLibraryRootPath = new FileReference(AdditionalLibrary).Directory.MakeRelativeTo(Unreal.RootDirectory);

						// Figure out how many dirs we need to go back
						string RelativeRootPath = Unreal.RootDirectory.MakeRelativeTo(OutputFile.Location.Directory);

						// Combine the two together ie. number of ../ + the path after the root
						RelativePath = Path.Combine(RelativeRootPath, AdditionalLibraryRootPath);
					}

					// On Windows, MakeRelativeTo can silently fail if the engine and the project are located on different drives
					if (CrossCompiling() && RelativePath.StartsWith(Unreal.RootDirectory.FullName))
					{
						// do not replace directly, but take care to avoid potential double slashes or missed slashes
						string PathFromRootDir = RelativePath.Replace(Unreal.RootDirectory.FullName, "");
						// Path.Combine doesn't combine these properly
						RelativePath = ((PathFromRootDir.StartsWith("\\") || PathFromRootDir.StartsWith("/")) ? "..\\..\\.." : "..\\..\\..\\") + PathFromRootDir;
					}

					if (!RPaths.Contains(RelativePath))
					{
						RPaths.Add(RelativePath);
						ResponseLines.Add(string.Format(" -rpath=\"${{ORIGIN}}/{0}\"", RelativePath.Replace('\\', '/')));
					}
				}
			}

			foreach (string RuntimeLibaryPath in LinkEnvironment.RuntimeLibraryPaths)
			{
				string RelativePath = RuntimeLibaryPath;

				if (!RelativePath.StartsWith("$"))
				{
					if (LinkEnvironment.bIsBuildingDLL)
					{
						// Remove the root Unreal.RootDirectory from the RuntimeLibaryPath
						string RuntimeLibraryRootPath = new DirectoryReference(RuntimeLibaryPath).MakeRelativeTo(Unreal.RootDirectory);

						// Figure out how many dirs we need to go back
						string RelativeRootPath = Unreal.RootDirectory.MakeRelativeTo(OutputFile.Location.Directory);

						// Combine the two together ie. number of ../ + the path after the root
						RelativePath = Path.Combine(RelativeRootPath, RuntimeLibraryRootPath);
					}
					else
					{
						string RelativeRootPath = new DirectoryReference(RuntimeLibaryPath).MakeRelativeTo(Unreal.RootDirectory);

						// We're assuming that the binary will be placed according to our ProjectName/Binaries/Platform scheme
						RelativePath = Path.Combine("..", "..", "..", RelativeRootPath);
					}
				}

				// On Windows, MakeRelativeTo can silently fail if the engine and the project are located on different drives
				if (CrossCompiling() && RelativePath.StartsWith(Unreal.RootDirectory.FullName))
				{
					// do not replace directly, but take care to avoid potential double slashes or missed slashes
					string PathFromRootDir = RelativePath.Replace(Unreal.RootDirectory.FullName, "");
					// Path.Combine doesn't combine these properly
					RelativePath = ((PathFromRootDir.StartsWith("\\") || PathFromRootDir.StartsWith("/")) ? "..\\..\\.." : "..\\..\\..\\") + PathFromRootDir;
				}

				if (!RPaths.Contains(RelativePath))
				{
					RPaths.Add(RelativePath);
					ResponseLines.Add(string.Format(" -rpath=\"${{ORIGIN}}/{0}\"", RelativePath.Replace('\\', '/')));
				}
			}

			ResponseLines.Add(string.Format(" -rpath-link=\"{0}\"", Path.GetDirectoryName(OutputFile.AbsolutePath)));

			// Add the library paths to the argument list.
			foreach (DirectoryReference LibraryPath in AllLibraryPaths)
			{
				// use absolute paths because of FixDependencies script again
				ResponseLines.Add(string.Format(" -L\"{0}\"", LibraryPath.FullName.Replace('\\', '/')));
			}

			List<string> EngineAndGameLibrariesLinkFlags = new List<string>();
			List<FileItem> EngineAndGameLibrariesFiles = new List<FileItem>();

			// Pre-2.25 ld has symbol resolution problems when .so are mixed with .a in a single --start-group/--end-group
			// when linking with --as-needed.
			// Move external libraries to a separate --start-group/--end-group to fix it (and also make groups smaller and faster to link).
			// See https://github.com/EpicGames/UnrealEngine/pull/2778 and https://github.com/EpicGames/UnrealEngine/pull/2793 for discussion
			string ExternalLibraries = "";

			// add libraries in a library group
			ResponseLines.Add(string.Format(" --start-group"));

			foreach (string AdditionalLibrary in AdditionalLibraries)
			{
				if (String.IsNullOrEmpty(Path.GetDirectoryName(AdditionalLibrary)))
				{
					// library was passed just like "jemalloc", turn it into -ljemalloc
					ExternalLibraries += string.Format(" -l{0}", AdditionalLibrary);
				}
				else if (Path.GetExtension(AdditionalLibrary) == ".a")
				{
					// static library passed in, pass it along but make path absolute, because FixDependencies script may be executed in a different directory
					string AbsoluteAdditionalLibrary = Path.GetFullPath(AdditionalLibrary);
					if (AbsoluteAdditionalLibrary.Contains(" "))
					{
						AbsoluteAdditionalLibrary = string.Format("\"{0}\"", AbsoluteAdditionalLibrary);
					}
					AbsoluteAdditionalLibrary = AbsoluteAdditionalLibrary.Replace('\\', '/');

					// libcrypto/libssl contain number of functions that are being used in different DSOs. FIXME: generalize?
					if (LinkEnvironment.bIsBuildingDLL && (AbsoluteAdditionalLibrary.Contains("libcrypto") || AbsoluteAdditionalLibrary.Contains("libssl")))
					{
						ResponseLines.Add(" --whole-archive " + AbsoluteAdditionalLibrary + " --no-whole-archive");
					}
					else
					{
						ResponseLines.Add(" " + AbsoluteAdditionalLibrary);
					}

					LinkAction.PrerequisiteItems.Add(FileItem.GetItemByPath(AdditionalLibrary));
				}
				else
				{
					// Skip over full-pathed library dependencies when building DLLs to avoid circular
					// dependencies.
					FileItem LibraryDependency = FileItem.GetItemByPath(AdditionalLibrary);

					string LibName = Path.GetFileNameWithoutExtension(AdditionalLibrary);
					if (LibName.StartsWith("lib"))
					{
						// Remove lib prefix
						LibName = LibName.Remove(0, 3);
					}
					else if (LibraryDependency.Exists)
					{
						// The library exists, but it is not prefixed with "lib", so force the
						// linker to find it without that prefix by prepending a colon to
						// the file name.
						LibName = string.Format(":{0}", LibraryDependency.Name);
					}
					string LibLinkFlag = string.Format(" -l{0}", LibName);

					if (LinkEnvironment.bIsBuildingDLL && LinkEnvironment.bIsCrossReferenced)
					{
						// We are building a cross referenced DLL so we can't actually include
						// dependencies at this point. Instead we add it to the list of
						// libraries to be used in the FixDependencies step.
						EngineAndGameLibrariesLinkFlags.Add(LibLinkFlag);
						EngineAndGameLibrariesFiles.Add(LibraryDependency);
						// it is important to add this exactly to the same place where the missing libraries would have been, it will be replaced later
						if (!ExternalLibraries.Contains("--allow-shlib-undefined"))
						{
							ExternalLibraries += string.Format(" -Wl,--allow-shlib-undefined");
						}
					}
					else
					{
						LinkAction.PrerequisiteItems.Add(LibraryDependency);
						ExternalLibraries += LibLinkFlag;
					}
				}
			}
			ResponseLines.Add(" --end-group");

			FileReference ResponseFileName = GetResponseFileName(LinkEnvironment, OutputFile);
			FileItem ResponseFileItem = Graph.CreateIntermediateTextFile(ResponseFileName, ResponseLines);

			LinkCommandString += string.Format(" -Wl,@\"{0}\"", ResponseFileName);
			LinkAction.PrerequisiteItems.Add(ResponseFileItem);

			LinkCommandString += " -Wl,--start-group";
			LinkCommandString += ExternalLibraries;

			// make unresolved symbols an error, unless a) building a cross-referenced DSO  b) we opted out
			if ((!LinkEnvironment.bIsBuildingDLL || !LinkEnvironment.bIsCrossReferenced) && !LinkEnvironment.bIgnoreUnresolvedSymbols)
			{
				// This will make the linker report undefined symbols the current module, but ignore in the dependent DSOs.
				// It is tempting, but may not be possible to change that report-all - due to circular dependencies between our libs.
				LinkCommandString += " -Wl,--unresolved-symbols=ignore-in-shared-libs";
			}
			LinkCommandString += " -Wl,--end-group";

			LinkCommandString += " -lrt"; // needed for clock_gettime()
			LinkCommandString += " -lm"; // math

			if (ShouldUseLibcxx(LinkEnvironment.Architecture))
			{
				// libc++ and its abi lib
				LinkCommandString += " -nodefaultlibs";
				LinkCommandString += " -L" + "ThirdParty/Unix/LibCxx/lib/Unix/" + LinkEnvironment.Architecture + "/";
				LinkCommandString += " " + "ThirdParty/Unix/LibCxx/lib/Unix/" + LinkEnvironment.Architecture + "/libc++.a";
				LinkCommandString += " " + "ThirdParty/Unix/LibCxx/lib/Unix/" + LinkEnvironment.Architecture + "/libc++abi.a";
				LinkCommandString += " -lm";
				LinkCommandString += " -lc";
				LinkCommandString += " -lpthread"; // pthread_mutex_trylock is missing from libc stubs
				LinkCommandString += " -lgcc_s";
				LinkCommandString += " -lgcc";
			}

			// these can be helpful for understanding the order of libraries or library search directories
			if (PlatformSDK.bVerboseLinker)
			{
				LinkCommandString += " -Wl,--verbose";
				LinkCommandString += " -Wl,--trace";
				LinkCommandString += " -v";
			}

			// Add the additional arguments specified by the environment.
			LinkCommandString += LinkEnvironment.AdditionalArguments;
			LinkCommandString = LinkCommandString.Replace("\\\\", "/");
			LinkCommandString = LinkCommandString.Replace("\\", "/");

			bool bUseCmdExe = BuildHostPlatform.Current.ShellType == ShellType.Cmd;
			FileReference ShellBinary = BuildHostPlatform.Current.Shell;
			string ExecuteSwitch = bUseCmdExe ? " /C" : ""; // avoid -c so scripts don't need +x

			// Linux has issues with scripts and parameter expansion from curely brakets
			if (!bUseCmdExe)
			{
				LinkCommandString = LinkCommandString.Replace("{", "'{");
				LinkCommandString = LinkCommandString.Replace("}", "}'");
				LinkCommandString = LinkCommandString.Replace("$'{", "'${");    // fixing $'{ORIGIN}' to be '${ORIGIN}'
			}

			string LinkScriptName = string.Format((bUseCmdExe ? "Link-{0}.link.bat" : "Link-{0}.link.sh"), OutputFile.Location.GetFileName());
			string LinkScriptFullPath = Path.Combine(LinkEnvironment.LocalShadowDirectory!.FullName, LinkScriptName);
			Logger.LogDebug("Creating link script: {LinkScriptFullPath}", LinkScriptFullPath);
			Directory.CreateDirectory(Path.GetDirectoryName(LinkScriptFullPath)!);
			using (StreamWriter LinkWriter = File.CreateText(LinkScriptFullPath))
			{
				if (bUseCmdExe)
				{
					LinkWriter.NewLine = "\r\n";
					LinkWriter.WriteLine("@echo off");
					LinkWriter.WriteLine("rem Automatically generated by UnrealBuildTool");
					LinkWriter.WriteLine("rem *DO NOT EDIT*");
					LinkWriter.WriteLine();
					LinkWriter.WriteLine("set Retries=0");
					LinkWriter.WriteLine(":linkloop");
					LinkWriter.WriteLine("if %Retries% GEQ 10 goto failedtorelink");
					LinkWriter.WriteLine(LinkCommandString);
					LinkWriter.WriteLine("if %errorlevel% neq 0 goto sleepandretry");
					LinkWriter.WriteLine(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile));
					LinkWriter.WriteLine("exit 0");
					LinkWriter.WriteLine(":sleepandretry");
					LinkWriter.WriteLine("ping 127.0.0.1 -n 1 -w 5000 >NUL 2>NUL");     // timeout complains about lack of redirection
					LinkWriter.WriteLine("set /a Retries+=1");
					LinkWriter.WriteLine("goto linkloop");
					LinkWriter.WriteLine(":failedtorelink");
					LinkWriter.WriteLine("echo Failed to link {0} after %Retries% retries", OutputFile.AbsolutePath);
					LinkWriter.WriteLine("exit 1");
				}
				else
				{
					LinkWriter.NewLine = "\n";
					LinkWriter.WriteLine("#!/bin/sh");
					LinkWriter.WriteLine("# Automatically generated by UnrealBuildTool");
					LinkWriter.WriteLine("# *DO NOT EDIT*");
					LinkWriter.WriteLine();
					LinkWriter.WriteLine("set -o errexit");
					LinkWriter.WriteLine(LinkCommandString);
					LinkWriter.WriteLine(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile));
				}
			};

			LinkAction.CommandPath = ShellBinary;

			// This must maintain the quotes around the LinkScriptFullPath
			LinkAction.CommandArguments = ExecuteSwitch + " \"" + LinkScriptFullPath + "\"";

			// prepare a linker script
			FileReference LinkerScriptPath = FileReference.Combine(LinkEnvironment.LocalShadowDirectory, "remove-sym.ldscript");
			if (!DirectoryReference.Exists(LinkEnvironment.LocalShadowDirectory))
			{
				DirectoryReference.CreateDirectory(LinkEnvironment.LocalShadowDirectory);
			}
			if (FileReference.Exists(LinkerScriptPath))
			{
				FileReference.Delete(LinkerScriptPath);
			}

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			// Prepare a script that will run later, once all shared libraries and the executable
			// are created. This script will be called by action created in FixDependencies()
			if (LinkEnvironment.bIsCrossReferenced && LinkEnvironment.bIsBuildingDLL)
			{
				if (bUseFixdeps)
				{
					string ScriptName = bUseCmdExe ? "FixDependencies.bat" : "FixDependencies.sh";

					string FixDepsScriptPath = Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, ScriptName);
					if (!bHasWipedFixDepsScript)
					{
						bHasWipedFixDepsScript = true;
						Logger.LogDebug("Creating script: {FixDepsScriptPath}", FixDepsScriptPath);
						Directory.CreateDirectory(Path.GetDirectoryName(FixDepsScriptPath)!);
						using (StreamWriter Writer = File.CreateText(FixDepsScriptPath))
						{
							if (bUseCmdExe)
							{
								Writer.NewLine = "\r\n";
								Writer.WriteLine("@echo off");
								Writer.WriteLine("rem Automatically generated by UnrealBuildTool");
								Writer.WriteLine("rem *DO NOT EDIT*");
								Writer.WriteLine();
							}
							else
							{
								Writer.NewLine = "\n";
								Writer.WriteLine("#!/bin/sh");
								Writer.WriteLine("# Automatically generated by UnrealBuildTool");
								Writer.WriteLine("# *DO NOT EDIT*");
								Writer.WriteLine();
								Writer.WriteLine("set -o errexit");
							}
						}
					}

					StreamWriter FixDepsScript = File.AppendText(FixDepsScriptPath);
					FixDepsScript.NewLine = bUseCmdExe ? "\r\n" : "\n";

					string EngineAndGameLibrariesString = "";
					foreach (string Library in EngineAndGameLibrariesLinkFlags)
					{
						EngineAndGameLibrariesString += Library;
					}

					FixDepsScript.WriteLine("echo Fixing {0}", Path.GetFileName(OutputFile.AbsolutePath));
					if (!bUseCmdExe)
					{
						FixDepsScript.WriteLine("TIMESTAMP=`stat --format %y \"{0}\"`", OutputFile.AbsolutePath);
					}
					string FixDepsLine = LinkCommandString;
					string Replace = "-Wl,--allow-shlib-undefined";

					FixDepsLine = FixDepsLine.Replace(Replace, EngineAndGameLibrariesString);
					string OutputFileForwardSlashes = OutputFile.AbsolutePath.Replace("\\", "/");
					FixDepsLine = FixDepsLine.Replace(OutputFileForwardSlashes, OutputFileForwardSlashes + ".fixed");
					FixDepsLine = FixDepsLine.Replace("$", "\\$");
					FixDepsScript.WriteLine(FixDepsLine);
					if (bUseCmdExe)
					{
						FixDepsScript.WriteLine("move /Y \"{0}.fixed\" \"{0}\"", OutputFile.AbsolutePath);
					}
					else
					{
						FixDepsScript.WriteLine("mv \"{0}.fixed\" \"{0}\"", OutputFile.AbsolutePath);
						FixDepsScript.WriteLine("touch -d \"$TIMESTAMP\" \"{0}\"", OutputFile.AbsolutePath);
						FixDepsScript.WriteLine();
					}
					FixDepsScript.Close();
				}
				else
				{
					// Create the action to relink the library. This actions does not overwrite the source file so it can be executed in parallel
					Action RelinkAction = Graph.CreateAction(ActionType.Link);
					RelinkAction.WorkingDirectory = LinkAction.WorkingDirectory;
					RelinkAction.StatusDescription = LinkAction.StatusDescription;
					RelinkAction.CommandDescription = "Relink";
					RelinkAction.bCanExecuteRemotely = false;
					RelinkAction.ProducedItems.Clear();
					RelinkAction.PrerequisiteItems = new List<FileItem>(LinkAction.PrerequisiteItems);
					foreach (FileItem Dependency in EngineAndGameLibrariesFiles)
					{
						RelinkAction.PrerequisiteItems.Add(Dependency);
					}
					RelinkAction.PrerequisiteItems.Add(OutputFile); // also depend on the first link action's output

					string LinkOutputFileForwardSlashes = OutputFile.AbsolutePath.Replace("\\", "/");
					string RelinkedFileForwardSlashes = Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, OutputFile.Location.GetFileName()) + ".relinked";

					// cannot use the real product because we need to maintain the timestamp on it
					FileReference RelinkActionDummyProductRef = FileReference.Combine(LinkEnvironment.LocalShadowDirectory, LinkEnvironment.OutputFilePath.GetFileNameWithoutExtension() + ".relinked_action_ran");
					RelinkAction.ProducedItems.Add(FileItem.GetItemByFileReference(RelinkActionDummyProductRef));

					string EngineAndGameLibrariesString = "";
					foreach (string Library in EngineAndGameLibrariesLinkFlags)
					{
						EngineAndGameLibrariesString += Library;
					}

					// create the relinking step
					string RelinkScriptName = string.Format((bUseCmdExe ? "Relink-{0}.bat" : "Relink-{0}.sh"), OutputFile.Location.GetFileName());
					string RelinkScriptFullPath = Path.Combine(LinkEnvironment.LocalShadowDirectory.FullName, RelinkScriptName);

					Logger.LogDebug("Creating script: {RelinkScriptFullPath}", RelinkScriptFullPath);
					Directory.CreateDirectory(Path.GetDirectoryName(RelinkScriptFullPath)!);
					using (StreamWriter RelinkWriter = File.CreateText(RelinkScriptFullPath))
					{
						string RelinkInvocation = LinkCommandString;
						string Replace = "-Wl,--allow-shlib-undefined";
						RelinkInvocation = RelinkInvocation.Replace(Replace, EngineAndGameLibrariesString);

						// should be the same as RelinkedFileRef
						RelinkInvocation = RelinkInvocation.Replace(LinkOutputFileForwardSlashes, RelinkedFileForwardSlashes);
						RelinkInvocation = RelinkInvocation.Replace("$", "\\$");

						if (bUseCmdExe)
						{
							RelinkWriter.WriteLine("@echo off");
							RelinkWriter.WriteLine("rem Automatically generated by UnrealBuildTool");
							RelinkWriter.WriteLine("rem *DO NOT EDIT*");
							RelinkWriter.WriteLine();
							RelinkWriter.WriteLine("set Retries=0");
							RelinkWriter.WriteLine(":relinkloop");
							RelinkWriter.WriteLine("if %Retries% GEQ 10 goto failedtorelink");
							RelinkWriter.WriteLine(RelinkInvocation);
							RelinkWriter.WriteLine("if %errorlevel% neq 0 goto sleepandretry");
							RelinkWriter.WriteLine("copy /B \"{0}\" \"{1}.temp\" >NUL 2>NUL", RelinkedFileForwardSlashes, OutputFile.AbsolutePath);
							RelinkWriter.WriteLine("if %errorlevel% neq 0 goto sleepandretry");
							RelinkWriter.WriteLine("move /Y \"{0}.temp\" \"{1}\" >NUL 2>NUL", OutputFile.AbsolutePath, OutputFile.AbsolutePath);
							RelinkWriter.WriteLine("if %errorlevel% neq 0 goto sleepandretry");
							RelinkWriter.WriteLine(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile));
							RelinkWriter.WriteLine("echo \"Dummy\" >> \"{0}\" && copy /b \"{0}\" +,,", RelinkActionDummyProductRef.FullName);
							RelinkWriter.WriteLine("echo Relinked {0} successfully after %Retries% retries", OutputFile.AbsolutePath);
							RelinkWriter.WriteLine("exit 0");
							RelinkWriter.WriteLine(":sleepandretry");
							RelinkWriter.WriteLine("ping 127.0.0.1 -n 1 -w 5000 >NUL 2>NUL");     // timeout complains about lack of redirection
							RelinkWriter.WriteLine("set /a Retries+=1");
							RelinkWriter.WriteLine("goto relinkloop");
							RelinkWriter.WriteLine(":failedtorelink");
							RelinkWriter.WriteLine("echo Failed to relink {0} after %Retries% retries", OutputFile.AbsolutePath);
							RelinkWriter.WriteLine("exit 1");
						}
						else
						{
							RelinkWriter.NewLine = "\n";
							RelinkWriter.WriteLine("#!/bin/sh");
							RelinkWriter.WriteLine("# Automatically generated by UnrealBuildTool");
							RelinkWriter.WriteLine("# *DO NOT EDIT*");
							RelinkWriter.WriteLine();
							RelinkWriter.WriteLine("set -o errexit");
							RelinkWriter.WriteLine(RelinkInvocation);
							RelinkWriter.WriteLine("TIMESTAMP=`stat --format %y \"{0}\"`", OutputFile.AbsolutePath);
							RelinkWriter.WriteLine("cp \"{0}\" \"{1}.temp\"", RelinkedFileForwardSlashes, OutputFile.AbsolutePath);
							RelinkWriter.WriteLine("mv \"{0}.temp\" \"{1}\"", OutputFile.AbsolutePath, OutputFile.AbsolutePath);
							RelinkWriter.WriteLine(GetDumpEncodeDebugCommand(LinkEnvironment, OutputFile));
							RelinkWriter.WriteLine("touch -d \"$TIMESTAMP\" \"{0}\"", OutputFile.AbsolutePath);
							RelinkWriter.WriteLine();
							RelinkWriter.WriteLine("echo \"Dummy\" >> \"{0}\"", RelinkActionDummyProductRef.FullName);
						}
					}

					RelinkAction.CommandPath = ShellBinary;
					RelinkAction.CommandArguments = ExecuteSwitch + " \"" + RelinkScriptFullPath + "\"";
				}
			}
			return OutputFile;
		}

		public override void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName)
		{
			if (bUseFixdeps)
			{
				foreach (UEBuildBinary Binary in Binaries)
				{
					AllBinaries.Add(FileItem.GetItemByFileReference(Binary.OutputFilePath));
				}
			}
		}

		public override ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment BinaryLinkEnvironment, IActionGraphBuilder Graph)
		{
			ICollection<FileItem> OutputFiles = base.PostBuild(Executable, BinaryLinkEnvironment, Graph);

			if (bUseFixdeps)
			{
				if (BinaryLinkEnvironment.bIsBuildingDLL || BinaryLinkEnvironment.bIsBuildingLibrary)
				{
					return OutputFiles;
				}

				FileItem? FixDepsOutputFile = FixDependencies(BinaryLinkEnvironment, Executable, Graph, Logger);
				if (FixDepsOutputFile != null)
				{
					OutputFiles.Add(FixDepsOutputFile);
				}
			}
			else
			{
				// make build product of cross-referenced DSOs to be *.relinked_action_ran, so the relinking steps are executed
				if (BinaryLinkEnvironment.bIsBuildingDLL && BinaryLinkEnvironment.bIsCrossReferenced)
				{
					FileReference RelinkedMapRef = FileReference.Combine(BinaryLinkEnvironment.LocalShadowDirectory!, BinaryLinkEnvironment.OutputFilePath.GetFileNameWithoutExtension() + ".relinked_action_ran");
					OutputFiles.Add(FileItem.GetItemByFileReference(RelinkedMapRef));
				}
			}
			return OutputFiles;
		}

		public void StripSymbols(FileReference SourceFile, FileReference TargetFile, ILogger Logger)
		{
			if (SourceFile != TargetFile)
			{
				// Strip command only works in place so we need to copy original if target is different
				File.Copy(SourceFile.FullName, TargetFile.FullName, true);
			}

			ProcessStartInfo StartInfo = new ProcessStartInfo();
			StartInfo.FileName = LinuxInfo.Objcopy.FullName;
			StartInfo.Arguments = "--strip-debug \"" + TargetFile.FullName + "\"";
			StartInfo.UseShellExecute = false;
			StartInfo.CreateNoWindow = true;
			Utils.RunLocalProcessAndLogOutput(StartInfo, Logger);
		}

		public override void AddExtraToolArguments(IList<string> ExtraArguments)
		{
			// We explicitly include the clang include directories so tools like IWYU can run outside of the clang directory.
			// More info: https://github.com/include-what-you-use/include-what-you-use#how-to-install
			string? InternalSdkPath = UEBuildPlatform.GetSDK(UnrealTargetPlatform.Linux)!.GetInternalSDKPath();
			if (InternalSdkPath != null)
			{
				ExtraArguments.Add(String.Format("-isystem\"{0}\"", System.IO.Path.Combine(InternalSdkPath, "lib/clang/" + Info.ClangVersion + "/include/").Replace("\\", "/")));
			}
		}
	}
}
