// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.Linq;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;
using System.IO;
using System.Text.RegularExpressions;

namespace UnrealBuildTool
{
	/// <summary>
	/// Common option flags for the Clang toolchains.
	/// Usage of these flags is currently inconsistent between various toolchains.
	/// </summary>
	[Flags]
	enum ClangToolChainOptions
	{
		/// <summary>
		/// No custom options
		/// </summary>
		None = 0,

		/// <summary>
		/// Enable address sanitzier
		/// </summary>
		EnableAddressSanitizer = 1 << 0,

		/// <summary>
		/// Enable hardware address sanitzier
		/// </summary>
		EnableHWAddressSanitizer = 1 << 1,

		/// <summary>
		/// Enable thread sanitizer
		/// </summary>
		EnableThreadSanitizer = 1 << 2,

		/// <summary>
		/// Enable undefined behavior sanitizer
		/// </summary>
		EnableUndefinedBehaviorSanitizer = 1 << 3,

		/// <summary>
		/// Enable minimal undefined behavior sanitizer
		/// </summary>
		EnableMinimalUndefinedBehaviorSanitizer = 1 << 4,

		/// <summary>
		/// Enable memory sanitizer
		/// </summary>
		EnableMemorySanitizer = 1 << 5,

		/// <summary>
		/// Enable Shared library for the Sanitizers otherwise defaults to Statically linked
		/// </summary>
		EnableSharedSanitizer = 1 << 6,

		/// <summary>
		/// Enables link time optimization (LTO). Link times will significantly increase.
		/// </summary>
		EnableLinkTimeOptimization = 1 << 7,

		/// <summary>
		/// Enable thin LTO
		/// </summary>
		EnableThinLTO = 1 << 8,

		/// <summary>
		/// If should disable using objcopy to split the debug info into its own file or now
		/// When we support larger the 4GB files with objcopy.exe this can be removed!
		/// </summary>
		DisableSplitDebugInfoWithObjCopy = 1 << 9,

		/// <summary>
		/// Enable tuning of debug info for LLDB
		/// </summary>
		TuneDebugInfoForLLDB = 1 << 10,

		/// <summary>
		/// Whether or not to preserve the portable symbol file produced by dump_syms
		/// </summary>
		PreservePSYM = 1 << 11,

		/// <summary>
		/// (Apple toolchains) Whether we're outputting a dylib instead of an executable
		/// </summary>
		OutputDylib = 1 << 12,

		/// <summary>
		/// Enables the creation of custom symbol files used for runtime symbol resolution.
		/// </summary>
		GenerateSymbols = 1 << 13,

		/// <summary>
		/// Enables live code editing
		/// </summary>
		EnableLiveCodeEditing = 1 << 14,

		/// <summary>
		/// Enables dead code/data stripping and common code folding.
		/// </summary>
		EnableDeadStripping = 1 << 15,

		/// <summary>
		/// Indicates that the target is a moduler build i.e. Target.LinkType == TargetLinkType.Modular
		/// </summary>
		ModularBuild = 1 << 16,
	}

	abstract class ClangToolChain : ISPCToolChain
	{
		protected class ClangToolChainInfo
		{
			protected ILogger Logger { get; init; }
			public FileReference Clang { get; init; }
			public FileReference Archiver { get; init; }

			public Version ClangVersion => LazyClangVersion.Value;
			public string ClangVersionString => LazyClangVersionString.Value;
			public string ArchiverVersionString => LazyArchiverVersionString.Value;

			Lazy<Version> LazyClangVersion;
			Lazy<string> LazyClangVersionString;
			Lazy<string> LazyArchiverVersionString;

			/// <summary>
			/// Constructor for ClangToolChainInfo
			/// </summary>
			/// <param name="Clang">The path to the compiler</param>
			/// <param name="Archiver">The path to the archiver</param>
			/// <param name="Logger">Logging interface</param>
			public ClangToolChainInfo(FileReference Clang, FileReference Archiver, ILogger Logger)
			{
				this.Logger = Logger;
				this.Clang = Clang;
				this.Archiver = Archiver;

				LazyClangVersion = new Lazy<Version>(() => QueryClangVersion());
				LazyClangVersionString = new Lazy<string>(() => QueryClangVersionString());
				LazyArchiverVersionString = new Lazy<string>(() => QueryArchiverVersionString());
			}

			/// <summary>
			/// Lazily query the clang version. Will only be executed once.
			/// </summary>
			/// <returns>The version of clang</returns>
			/// <exception cref="BuildException"></exception>
			protected virtual Version QueryClangVersion()
			{
				Match MatchClangVersion = Regex.Match(ClangVersionString, @"clang version (?<full>(?<major>\d+)\.(?<minor>\d+)\.(?<patch>\d+))");
				if (MatchClangVersion.Success && Version.TryParse(MatchClangVersion.Groups["full"].Value, out Version? ClangVersion))
				{
					return ClangVersion;
				}
				throw new BuildException("Failed to query the Clang version number!");
			}

			/// <summary>
			/// Lazily query the clang version output. Will only be executed once.
			/// </summary>
			/// <returns>The standard output when running Clang --version</returns>
			protected virtual string QueryClangVersionString() => Utils.RunLocalProcessAndReturnStdOut(Clang.FullName, "--version", null);

			/// <summary>
			/// Lazily query the archiver version output. Will only be executed once.
			/// </summary>
			/// <returns>The standard output when running Archiver --version</returns>
			protected virtual string QueryArchiverVersionString() => Utils.RunLocalProcessAndReturnStdOut(Archiver.FullName, "--version", null);
		}

		// The Clang version being used to compile
		Lazy<ClangToolChainInfo> LazyInfo;
		protected ClangToolChainInfo Info => LazyInfo.Value;

		protected ClangToolChainOptions Options;

		// Dummy define to work around clang compilation related to the windows maximum path length limitation
		protected static string ClangDummyDefine; 
		protected const int ClangCmdLineMaxSize = 32 * 1024;
		protected const int ClangCmdlineDangerZone = 30 * 1024;

		// Target settings
		protected bool PreprocessDepends = false;
		protected StaticAnalyzer StaticAnalyzer = StaticAnalyzer.None;
		protected StaticAnalyzerMode StaticAnalyzerMode = StaticAnalyzerMode.Deep;
		protected StaticAnalyzerOutputType StaticAnalyzerOutputType = StaticAnalyzerOutputType.Text;

		static ClangToolChain()
		{
			const string DummyStart = "-D \"DUMMY_DEFINE";
			const string DummyEnd = "\"";
			ClangDummyDefine = DummyStart + "_".PadRight(ClangCmdLineMaxSize - ClangCmdlineDangerZone - DummyStart.Length - DummyEnd.Length, 'X') + DummyEnd;
		}

		public ClangToolChain(ClangToolChainOptions InOptions, ILogger InLogger)
			: base(InLogger)
		{
			Options = InOptions;

			LazyInfo = new Lazy<ClangToolChainInfo>(() => GetToolChainInfo());
		}

		protected abstract ClangToolChainInfo GetToolChainInfo();

		public override void SetUpGlobalEnvironment(ReadOnlyTargetRules Target)
		{
			base.SetUpGlobalEnvironment(Target);

			PreprocessDepends = Target.bPreprocessDepends;
			StaticAnalyzer = Target.StaticAnalyzer;
			StaticAnalyzerMode = Target.StaticAnalyzerMode;
			StaticAnalyzerOutputType = Target.StaticAnalyzerOutputType;
		}

		public override void FinalizeOutput(ReadOnlyTargetRules Target, TargetMakefileBuilder MakefileBuilder)
		{
			if (Target.bPrintToolChainTimingInfo && Target.bParseTimingInfoForTracing)
			{
				TargetMakefile Makefile = MakefileBuilder.Makefile;
				List<IExternalAction> CompileActions = Makefile.Actions.Where(x => x.ActionType == ActionType.Compile && x.ProducedItems.Any(i => i.HasExtension(".json"))).ToList();
				List<FileItem> TimingJsonFiles = CompileActions.SelectMany(a => a.ProducedItems.Where(i => i.HasExtension(".json"))).ToList();

				// Handing generating aggregate timing information if we compiled more than one file.
				if (TimingJsonFiles.Count > 0)
				{
					// Generate the file manifest for the aggregator.
					FileReference ManifestFile = FileReference.Combine(Makefile.ProjectIntermediateDirectory, $"{Target.Name}TimingManifest.csv");
					if (!DirectoryReference.Exists(ManifestFile.Directory))
					{
						DirectoryReference.CreateDirectory(ManifestFile.Directory);
					}
					File.WriteAllLines(ManifestFile.FullName, TimingJsonFiles.Select(f => f.FullName.Remove(f.FullName.Length - ".json".Length)));

					FileItem AggregateOutputFile = FileItem.GetItemByFileReference(FileReference.Combine(Makefile.ProjectIntermediateDirectory, $"{Target.Name}.trace.csv"));
					FileItem HeadersOutputFile = FileItem.GetItemByFileReference(FileReference.Combine(Makefile.ProjectIntermediateDirectory, $"{Target.Name}.headers.csv"));
					List<string> AggregateActionArgs = new List<string>()
					{
						$"-ManifestFile={ManifestFile.FullName}",
						$"-AggregateFile={AggregateOutputFile.FullName}",
						$"-HeadersFile={HeadersOutputFile.FullName}",
					};

					Action AggregateTimingInfoAction = MakefileBuilder.CreateRecursiveAction<AggregateClangTimingInfo>(ActionType.ParseTimingInfo, string.Join(" ", AggregateActionArgs));
					AggregateTimingInfoAction.WorkingDirectory = Unreal.EngineSourceDirectory;
					AggregateTimingInfoAction.StatusDescription = $"Aggregating {TimingJsonFiles.Count} Timing File(s)";
					AggregateTimingInfoAction.bCanExecuteRemotely = false;
					AggregateTimingInfoAction.bCanExecuteRemotelyWithSNDBS = false;
					AggregateTimingInfoAction.PrerequisiteItems.AddRange(TimingJsonFiles);

					AggregateTimingInfoAction.ProducedItems.Add(AggregateOutputFile);
					AggregateTimingInfoAction.ProducedItems.Add(HeadersOutputFile);
					Makefile.OutputItems.AddRange(AggregateTimingInfoAction.ProducedItems);

					FileItem ArchiveOutputFile = FileItem.GetItemByFileReference(FileReference.Combine(Makefile.ProjectIntermediateDirectory, $"{Target.Name}.traces.zip"));
					List<string> ArchiveActionArgs = new List<string>()
					{
						$"-ManifestFile={ManifestFile.FullName}",
						$"-ArchiveFile={ArchiveOutputFile.FullName}",
					};

					Action ArchiveTimingInfoAction = MakefileBuilder.CreateRecursiveAction<AggregateClangTimingInfo>(ActionType.ParseTimingInfo, string.Join(" ", ArchiveActionArgs));
					ArchiveTimingInfoAction.WorkingDirectory = Unreal.EngineSourceDirectory;
					ArchiveTimingInfoAction.StatusDescription = $"Archiving {TimingJsonFiles.Count} Timing File(s)";
					ArchiveTimingInfoAction.bCanExecuteRemotely = false;
					ArchiveTimingInfoAction.bCanExecuteRemotelyWithSNDBS = false;
					ArchiveTimingInfoAction.PrerequisiteItems.AddRange(TimingJsonFiles);

					ArchiveTimingInfoAction.ProducedItems.Add(ArchiveOutputFile);
					Makefile.OutputItems.AddRange(ArchiveTimingInfoAction.ProducedItems);

					// Extract CompileScore data from traces
					FileReference ScoreDataExtractor = FileReference.Combine(Unreal.RootDirectory, "Engine", "Extras", "ThirdPartyNotUE", "CompileScore", "ScoreDataExtractor.exe");
					if (RuntimePlatform.IsWindows && FileReference.Exists(ScoreDataExtractor))
					{
						FileItem CompileScoreOutput = FileItem.GetItemByFileReference(FileReference.Combine(Makefile.ProjectIntermediateDirectory, $"{Target.Name}.scor"));

						Action CompileScoreExtractorAction = MakefileBuilder.CreateAction(ActionType.ParseTimingInfo);
						CompileScoreExtractorAction.WorkingDirectory = Unreal.EngineSourceDirectory;
						CompileScoreExtractorAction.StatusDescription = $"Extracting CompileScore";
						CompileScoreExtractorAction.bCanExecuteRemotely = false;
						CompileScoreExtractorAction.bCanExecuteRemotelyWithSNDBS = false;
						CompileScoreExtractorAction.PrerequisiteItems.AddRange(TimingJsonFiles);
						CompileScoreExtractorAction.CommandPath = ScoreDataExtractor;
						CompileScoreExtractorAction.CommandArguments = $"-clang -verbosity 0 -timelinepack 1000000 -extract -i \"{NormalizeCommandLinePath(Makefile.ProjectIntermediateDirectory)}\" -o \"{NormalizeCommandLinePath(CompileScoreOutput)}\"";

						CompileScoreExtractorAction.ProducedItems.Add(CompileScoreOutput);
						CompileScoreExtractorAction.ProducedItems.Add(FileItem.GetItemByFileReference(FileReference.Combine(Makefile.ProjectIntermediateDirectory, $"{Target.Name}.scor.gbl")));
						CompileScoreExtractorAction.ProducedItems.Add(FileItem.GetItemByFileReference(FileReference.Combine(Makefile.ProjectIntermediateDirectory, $"{Target.Name}.scor.incl")));
						CompileScoreExtractorAction.ProducedItems.Add(FileItem.GetItemByFileReference(FileReference.Combine(Makefile.ProjectIntermediateDirectory, $"{Target.Name}.scor.t0000")));
						Makefile.OutputItems.AddRange(CompileScoreExtractorAction.ProducedItems);
					}
				}
			}
		}

		/// <summary>
		/// Sanitizes a preprocessor definition argument if needed.
		/// </summary>
		/// <param name="Definition">A string in the format "foo=bar" or "foo".</param>
		/// <returns>An escaped string</returns>
		protected virtual string EscapePreprocessorDefinition(string Definition)
		{
			// By default don't modify preprocessor definition, handle in platform overrides.
			return Definition;
		}

		/// <summary>
		/// Checks if compiler version matches the requirements
		/// </summary>
		protected bool CompilerVersionGreaterOrEqual(int Major, int Minor, int Patch) => Info.ClangVersion >= new Version(Major, Minor, Patch);

		/// <summary>
		/// Checks if compiler version matches the requirements
		/// </summary>
		protected bool CompilerVersionLessThan(int Major, int Minor, int Patch) => Info.ClangVersion < new Version(Major, Minor, Patch);

		protected bool IsAnalyzing(CppCompileEnvironment CompileEnvironment) => 
				StaticAnalyzer == StaticAnalyzer.Default 
			&&	CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create 
			&& !CompileEnvironment.StaticAnalyzerDisabledCheckers.Contains("all") 
			&& !CompileEnvironment.bDisableStaticAnalysis;

		protected virtual void GetCppStandardCompileArgument(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			// https://clang.llvm.org/cxx_status.html
			switch (CompileEnvironment.CppStandard)
			{
				case CppStandardVersion.Cpp14:
					Arguments.Add("-std=c++14");
					break;
				case CppStandardVersion.Cpp17:
					Arguments.Add("-std=c++17");
					break;
				case CppStandardVersion.Cpp20:
					Arguments.Add("-std=c++20");
					break;
				case CppStandardVersion.Latest:
					Arguments.Add(CompilerVersionLessThan(13, 0, 0) ? "-std=c++20" : "-std=c++2b");
					break;
				default:
					throw new BuildException($"Unsupported C++ standard type set: {CompileEnvironment.CppStandard}");
			}

			if (CompileEnvironment.bEnableCoroutines)
			{
				Arguments.Add("-fcoroutines-ts");
				if (!CompileEnvironment.bEnableExceptions)
				{
					Arguments.Add("-Wno-coroutine-missing-unhandled-exception");
				}
			}

			if (CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.None && CompilerVersionGreaterOrEqual(11, 0, 0))
			{
				// Validate PCH inputs by content if mtime check fails
				Arguments.Add("-fpch-validate-input-files-content");
			}
		}

		protected virtual void GetCStandardCompileArgument(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			switch (CompileEnvironment.CStandard)
			{
				case CStandardVersion.Default:
					break;
				case CStandardVersion.C89:
					Arguments.Add("-std=c89");
					break;
				case CStandardVersion.C99:
					Arguments.Add("-std=c99");
					break;
				case CStandardVersion.C11:
					Arguments.Add("-std=c11");
					break;
				case CStandardVersion.C17:
					Arguments.Add("-std=c17");
					break;
				case CStandardVersion.Latest:
					Arguments.Add("-std=c2x");
					break;
				default:
					throw new BuildException($"Unsupported C standard type set: {CompileEnvironment.CStandard}");
			}
		}

		protected virtual void GetCompileArguments_CPP(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x c++");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
		}

		protected virtual void GetCompileArguments_C(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x c");
			GetCStandardCompileArgument(CompileEnvironment, Arguments);
		}

		protected virtual void GetCompileArguments_MM(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c++");
			Arguments.Add("-fobjc-abi-version=2");
			Arguments.Add("-fobjc-legacy-dispatch");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
		}

		protected virtual void GetCompileArguments_M(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x objective-c");
			Arguments.Add("-fobjc-abi-version=2");
			Arguments.Add("-fobjc-legacy-dispatch");
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
		}

		protected virtual void GetCompileArguments_PCH(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-x c++-header");
			if (CompilerVersionGreaterOrEqual(11, 0, 0))
			{
				Arguments.Add("-fpch-instantiate-templates");
			}
			GetCppStandardCompileArgument(CompileEnvironment, Arguments);
		}

		// Conditionally enable (default disabled) generation of information about every class with virtual functions for use by the C++ runtime type identification features
		// (`dynamic_cast' and `typeid'). If you don't use those parts of the language, you can save some space by using -fno-rtti.
		// Note that exception handling uses the same information, but it will generate it as needed.
		protected virtual string GetRTTIFlag(CppCompileEnvironment CompileEnvironment)
		{
			return CompileEnvironment.bUseRTTI ? "-frtti" : "-fno-rtti";
		}

		protected virtual string GetUserIncludePathArgument(DirectoryReference IncludePath)
		{
			return $"-I\"{NormalizeCommandLinePath(IncludePath)}\"";
		}

		protected virtual string GetSystemIncludePathArgument(DirectoryReference IncludePath)
		{
			return $"-isystem\"{NormalizeCommandLinePath(IncludePath)}\"";
		}

		protected virtual void GetCompileArguments_IncludePaths(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.AddRange(CompileEnvironment.UserIncludePaths.Select(IncludePath => GetUserIncludePathArgument(IncludePath)));
			Arguments.AddRange(CompileEnvironment.SystemIncludePaths.Select(IncludePath => GetSystemIncludePathArgument(IncludePath)));
		}

		protected virtual string GetPreprocessorDefinitionArgument(string Definition)
		{
			return $"-D\"{EscapePreprocessorDefinition(Definition)}\"";
		}

		protected virtual void GetCompileArguments_PreprocessorDefinitions(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.AddRange(CompileEnvironment.Definitions.Select(Definition => GetPreprocessorDefinitionArgument(Definition)));

			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create && StaticAnalyzer == StaticAnalyzer.Default)
			{
				Arguments.Add(GetPreprocessorDefinitionArgument("__clang_analyzer__"));
			}
		}

		protected virtual string GetForceIncludeFileArgument(FileReference ForceIncludeFile)
		{
			return $"-include \"{NormalizeCommandLinePath(ForceIncludeFile)}\"";
		}

		protected virtual string GetForceIncludeFileArgument(FileItem ForceIncludeFile)
		{
			return GetForceIncludeFileArgument(ForceIncludeFile.Location);
		}

		protected virtual string GetIncludePCHFileArgument(FileReference IncludePCHFile)
		{
			return $"-include-pch \"{NormalizeCommandLinePath(IncludePCHFile)}\"";
		}

		protected virtual string GetIncludePCHFileArgument(FileItem IncludePCHFile)
		{
			return GetIncludePCHFileArgument(IncludePCHFile.Location);
		}

		protected virtual void GetCompileArguments_ForceInclude(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			// Add the precompiled header file's path to the force include path
			// This needs to be before the other force include paths to ensure clang uses it instead of the source header file.
			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				Arguments.Add(GetIncludePCHFileArgument(CompileEnvironment.PrecompiledHeaderFile!));
			}

			Arguments.AddRange(CompileEnvironment.ForceIncludeFiles.Select(ForceIncludeFile => GetForceIncludeFileArgument(ForceIncludeFile)));
		}

		protected virtual string GetSourceFileArgument(FileItem SourceFile)
		{
			return $"\"{NormalizeCommandLinePath(SourceFile)}\"";
		}

		protected virtual string GetOutputFileArgument(FileItem OutputFile)
		{
			return $"-o \"{NormalizeCommandLinePath(OutputFile)}\"";
		}

		protected virtual string GetDepencenciesListFileArgument(FileItem DependencyListFile)
		{
			return $"-MD -MF\"{NormalizeCommandLinePath(DependencyListFile)}\"";
		}

		protected virtual string GetPreprocessDepencenciesListFileArgument(FileItem DependencyListFile)
		{
			return $"-M -MF\"{NormalizeCommandLinePath(DependencyListFile)}\"";
		}

		protected virtual string GetResponseFileArgument(FileItem ResponseFile)
		{
			return $"@\"{NormalizeCommandLinePath(ResponseFile)}\"";
		}

		/// <summary>
		/// Common compile arguments that control which warnings are enabled.
		/// https://clang.llvm.org/docs/DiagnosticsReference.html
		/// </summary>
		/// <param name="CompileEnvironment"></param>
		/// <param name="Arguments"></param>
		protected virtual void GetCompileArguments_WarningsAndErrors(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-Wall");                                     // https://clang.llvm.org/docs/DiagnosticsReference.html#wall
			Arguments.Add("-Werror");                                   // https://clang.llvm.org/docs/UsersManual.html#cmdoption-werror

			Arguments.Add("-Wdelete-non-virtual-dtor");                 // https://clang.llvm.org/docs/DiagnosticsReference.html#wdelete-non-virtual-dtor
			Arguments.Add("-Wenum-conversion");                         // https://clang.llvm.org/docs/DiagnosticsReference.html#wenum-conversion
			Arguments.Add("-Wbitfield-enum-conversion");                // https://clang.llvm.org/docs/DiagnosticsReference.html#wbitfield-enum-conversion

			Arguments.Add("-Wno-enum-enum-conversion");                 // https://clang.llvm.org/docs/DiagnosticsReference.html#wenum-enum-conversion					// ?? no reason given
			Arguments.Add("-Wno-enum-float-conversion");                // https://clang.llvm.org/docs/DiagnosticsReference.html#wenum-float-conversion					// ?? no reason given

			if (CompilerVersionGreaterOrEqual(13, 0, 0))
			{
				Arguments.Add("-Wno-unused-but-set-variable");           // https://clang.llvm.org/docs/DiagnosticsReference.html#wunused-but-set-variable				// new warning for clang 13
				Arguments.Add("-Wno-unused-but-set-parameter");          // https://clang.llvm.org/docs/DiagnosticsReference.html#wunused-but-set-parameter				// new warning for clang 13
				Arguments.Add("-Wno-ordered-compare-function-pointers"); // https://clang.llvm.org/docs/DiagnosticsReference.html#wordered-compare-function-pointers	// new warning for clang 13
			}
			if (CompilerVersionGreaterOrEqual(14, 0, 0))
			{
				Arguments.Add("-Wno-bitwise-instead-of-logical");       // https://clang.llvm.org/docs/DiagnosticsReference.html#wbitwise-instead-of-logical			// new warning for clang 14
			}

			Arguments.Add("-Wno-gnu-string-literal-operator-template"); // https://clang.llvm.org/docs/DiagnosticsReference.html#wgnu-string-literal-operator-template	// We use this feature to allow static FNames.
			Arguments.Add("-Wno-inconsistent-missing-override");        // https://clang.llvm.org/docs/DiagnosticsReference.html#winconsistent-missing-override			// ?? no reason given
			Arguments.Add("-Wno-invalid-offsetof");                     // https://clang.llvm.org/docs/DiagnosticsReference.html#winvalid-offsetof						// needed to suppress warnings about using offsetof on non-POD types.
			Arguments.Add("-Wno-switch");                               // https://clang.llvm.org/docs/DiagnosticsReference.html#wswitch								// this hides the "enumeration value 'XXXXX' not handled in switch [-Wswitch]" warnings - we should maybe remove this at some point and add UE_LOG(, Fatal, ) to default cases
			Arguments.Add("-Wno-tautological-compare");                 // https://clang.llvm.org/docs/DiagnosticsReference.html#wtautological-compare					// this hides the "warning : comparison of unsigned expression < 0 is always false" type warnings due to constant comparisons, which are possible with template arguments
			Arguments.Add("-Wno-unknown-pragmas");                      // https://clang.llvm.org/docs/DiagnosticsReference.html#wunknown-pragmas						// Slate triggers this (with its optimize on/off pragmas)
			Arguments.Add("-Wno-unused-function");                      // https://clang.llvm.org/docs/DiagnosticsReference.html#wunused-function						// this will hide the warnings about static functions in headers that aren't used in every single .cpp file
			Arguments.Add("-Wno-unused-lambda-capture");                // https://clang.llvm.org/docs/DiagnosticsReference.html#wunused-lambda-capture					// suppressed because capturing of compile-time constants is seemingly inconsistent. And MSVC doesn't do that.
			Arguments.Add("-Wno-unused-local-typedef");                 // https://clang.llvm.org/docs/DiagnosticsReference.html#wunused-local-typedef					// clang is being overly strict here? PhysX headers trigger this.
			Arguments.Add("-Wno-unused-private-field");                 // https://clang.llvm.org/docs/DiagnosticsReference.html#wunused-private-field					// this will prevent the issue of warnings for unused private variables. MultichannelTcpSocket.h triggers this, possibly more
			Arguments.Add("-Wno-unused-variable");                      // https://clang.llvm.org/docs/DiagnosticsReference.html#wunused-variable						// ?? no reason given
			Arguments.Add("-Wno-undefined-var-template");               // https://clang.llvm.org/docs/DiagnosticsReference.html#wundefined-var-template				// not really a good warning to disable

			// Profile Guided Optimization (PGO) and Link Time Optimization (LTO)
			if (CompileEnvironment.bPGOOptimize)
			{
				//
				// Clang emits warnings for each compiled object file that doesn't have a matching entry in the profile data.
				// This can happen when the profile data is older than the binaries we're compiling.
				//
				// Disable these warnings. They are far too verbose.
				//
				Arguments.Add("-Wno-profile-instr-out-of-date");        // https://clang.llvm.org/docs/DiagnosticsReference.html#wprofile-instr-out-of-date
				Arguments.Add("-Wno-profile-instr-unprofiled");         // https://clang.llvm.org/docs/DiagnosticsReference.html#wprofile-instr-unprofiled

				// apparently there can be hashing conflicts with PGO which can result in:
				// 'Function control flow change detected (hash mismatch)' warnings. 
				Arguments.Add("-Wno-backend-plugin");                   // https://clang.llvm.org/docs/DiagnosticsReference.html#wbackend-plugin
			}

			// shipping builds will cause this warning with "ensure", so disable only in those case
			if (CompileEnvironment.Configuration == CppConfiguration.Shipping || StaticAnalyzer != StaticAnalyzer.None)
			{
				Arguments.Add("-Wno-unused-value");                     // https://clang.llvm.org/docs/DiagnosticsReference.html#wunused-value
			}

			// https://clang.llvm.org/docs/DiagnosticsReference.html#wdeprecated-declarations
			if (CompileEnvironment.DeprecationWarningLevel == WarningLevel.Error)
			{
				// TODO: This may be unnecessary with -Werror
				Arguments.Add("-Werror=deprecated-declarations");
			}

			// Warn if __DATE__ or __TIME__ are used as they prevent reproducible builds
			if (CompileEnvironment.bDeterministic)
			{
				Arguments.Add("-Wdate-time -Wno-error=date-time"); // https://clang.llvm.org/docs/DiagnosticsReference.html#wdate-time
			}

			// https://clang.llvm.org/docs/DiagnosticsReference.html#wshadow
			if (CompileEnvironment.ShadowVariableWarningLevel != WarningLevel.Off)
			{
				Arguments.Add("-Wshadow" + ((CompileEnvironment.ShadowVariableWarningLevel == WarningLevel.Error) ? "" : " -Wno-error=shadow"));
			}

			// https://clang.llvm.org/docs/DiagnosticsReference.html#wundef
			if (CompileEnvironment.bEnableUndefinedIdentifierWarnings)
			{
				Arguments.Add("-Wundef" + (CompileEnvironment.bUndefinedIdentifierWarningsAsErrors ? "" : " -Wno-error=undef"));
			}

			// Note: This should be kept in sync with PRAGMA_DISABLE_UNSAFE_TYPECAST_WARNINGS in ClangPlatformCompilerPreSetup.h
			string[] UnsafeTypeCastWarningList = {
				"float-conversion",
				"implicit-float-conversion",
				"implicit-int-conversion",
				"c++11-narrowing"
				//"shorten-64-to-32",	<-- too many hits right now, probably want it *soon*
				//"sign-conversion",	<-- too many hits right now, probably want it eventually
			};

			if (CompileEnvironment.UnsafeTypeCastWarningLevel == WarningLevel.Error)
			{
				foreach (string Warning in UnsafeTypeCastWarningList)
				{
					Arguments.Add("-W" + Warning);
				}
			}
			else if (CompileEnvironment.UnsafeTypeCastWarningLevel == WarningLevel.Warning)
			{
				foreach (string Warning in UnsafeTypeCastWarningList)
				{
					Arguments.Add("-W" + Warning + " -Wno-error=" + Warning);
				}
			}
			else
			{
				foreach (string Warning in UnsafeTypeCastWarningList)
				{
					Arguments.Add("-Wno-" + Warning);
				}
			}

			// always use absolute paths for errors, this can help IDEs go to the error properly
			Arguments.Add("-fdiagnostics-absolute-paths");  // https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-fdiagnostics-absolute-paths

			// https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-cla-fcolor-diagnostics
			if (Log.ColorConsoleOutput)
			{
				Arguments.Add("-fdiagnostics-color");
			}

			// https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-fdiagnostics-format
			if (RuntimePlatform.IsWindows)
			{
				Arguments.Add("-fdiagnostics-format=msvc");
			}

			// Set the output directory for crashes
			// https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-fcrash-diagnostics-dir
			DirectoryReference? CrashDiagnosticDirectory = DirectoryReference.FromString(CompileEnvironment.CrashDiagnosticDirectory);
			if (CrashDiagnosticDirectory != null)
			{
				if (DirectoryReference.Exists(CrashDiagnosticDirectory))
				{
					Arguments.Add($"-fcrash-diagnostics-dir=\"{NormalizeCommandLinePath(CrashDiagnosticDirectory)}\"");
				}
				else
				{
					Log.TraceWarningOnce("CrashDiagnosticDirectory has been specified but directory \"{CrashDiagnosticDirectory}\" does not exist. Compiler argument \"-fcrash-diagnostics-dir\" has been discarded.", CrashDiagnosticDirectory);
				}
			}
		}

		/// <summary>
		/// Compile arguments for optimization settings, such as profile guided optimization and link time optimization
		/// </summary>
		/// <param name="CompileEnvironment"></param>
		/// <param name="Arguments"></param>
		protected virtual void GetCompileArguments_Optimizations(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			// Profile Guided Optimization (PGO) and Link Time Optimization (LTO)
			if (CompileEnvironment.bPGOOptimize)
			{
				Log.TraceInformationOnce("Enabling Profile Guided Optimization (PGO). Linking will take a while.");
				Arguments.Add($"-fprofile-instr-use=\"{Path.Combine(CompileEnvironment.PGODirectory!, CompileEnvironment.PGOFilenamePrefix!)}\"");
			}
			else if (CompileEnvironment.bPGOProfile)
			{
				// Always enable LTO when generating PGO profile data.
				Log.TraceInformationOnce("Enabling Profile Guided Instrumentation (PGI). Linking will take a while.");
				Arguments.Add("-fprofile-generate");
			}

			if (!CompileEnvironment.bUseInlining)
			{
				Arguments.Add("-fno-inline-functions");
			}
		}

		/// <summary>
		/// Compile arguments for debug settings
		/// </summary>
		/// <param name="CompileEnvironment"></param>
		/// <param name="Arguments"></param>
		protected virtual void GetCompileArguments_Debugging(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			// Optionally enable exception handling (off by default since it generates extra code needed to propagate exceptions)
			if (CompileEnvironment.bEnableExceptions)
			{
				Arguments.Add("-fexceptions");
				Arguments.Add("-DPLATFORM_EXCEPTIONS_DISABLED=0");
			}
			else
			{
				Arguments.Add("-fno-exceptions");
				Arguments.Add("-DPLATFORM_EXCEPTIONS_DISABLED=1");
			}
		}

		/// <summary>
		/// Compile arguments for sanitizers
		/// </summary>
		/// <param name="CompileEnvironment"></param>
		/// <param name="Arguments"></param>
		protected virtual void GetCompilerArguments_Sanitizers(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			// ASan
			if (Options.HasFlag(ClangToolChainOptions.EnableAddressSanitizer))
			{
				Arguments.Add("-fsanitize=address");
			}

			// TSan
			if (Options.HasFlag(ClangToolChainOptions.EnableThreadSanitizer))
			{
				Arguments.Add("-fsanitize=thread");
			}

			// UBSan
			if (Options.HasFlag(ClangToolChainOptions.EnableUndefinedBehaviorSanitizer))
			{
				Arguments.Add("-fsanitize=undefined");
			}

			// MSan
			if (Options.HasFlag(ClangToolChainOptions.EnableMemorySanitizer))
			{
				Arguments.Add("-fsanitize=memory");
			}
		}

		/// <summary>
		/// Additional compile arguments.
		/// </summary>
		/// <param name="CompileEnvironment"></param>
		/// <param name="Arguments"></param>
		protected virtual void GetCompileArguments_AdditionalArgs(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			if (!string.IsNullOrWhiteSpace(CompileEnvironment.AdditionalArguments))
			{
				Arguments.Add(CompileEnvironment.AdditionalArguments);
			}
		}

		/// <summary>
		/// Compile arguments for running clang-analyze.
		/// </summary>
		/// <param name="CompileEnvironment"></param>
		/// <param name="Arguments"></param>
		protected virtual void GetCompileArguments_Analyze(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			Arguments.Add("-Wno-unused-command-line-argument");

			// Enable the static analyzer with default checks.
			Arguments.Add("--analyze");

			// Make sure we check inside nested blocks (e.g. 'if ((foo = getchar()) == 0) {}')
			Arguments.Add("-Xclang -analyzer-opt-analyze-nested-blocks");

			// Write out a pretty web page with navigation to understand how the analysis was derived if HTML is enabled.
			Arguments.Add($"-Xclang -analyzer-output={StaticAnalyzerOutputType.ToString().ToLowerInvariant()}");

			// Needed for some of the C++ checkers.
			Arguments.Add("-Xclang -analyzer-config -Xclang aggressive-binary-operation-simplification=true");

			// If writing to HTML, use the source filename as a basis for the report filename.
			Arguments.Add("-Xclang -analyzer-config -Xclang stable-report-filename=true");
			Arguments.Add("-Xclang -analyzer-config -Xclang report-in-main-source-file=true");
			Arguments.Add("-Xclang -analyzer-config -Xclang path-diagnostics-alternate=true");

			// Run shallow analyze if requested.
			if (StaticAnalyzerMode == StaticAnalyzerMode.Shallow) Arguments.Add("-Xclang -analyzer-config -Xclang mode=shallow");

			if (CompileEnvironment.StaticAnalyzerCheckers.Count > 0)
			{
				// Disable all default checks
				Arguments.Add("--analyzer-no-default-checks");

				// Only enable specific checkers.
				foreach (string Checker in CompileEnvironment.StaticAnalyzerCheckers)
				{
					Arguments.Add($"-Xclang -analyzer-checker -Xclang {Checker}");
				}
			}
			else
			{
				// Disable default checks.
				foreach (string Checker in CompileEnvironment.StaticAnalyzerDisabledCheckers)
				{
					Arguments.Add($"-Xclang -analyzer-disable-checker -Xclang {Checker}");
				}
				// Enable additional non-default checks.
				foreach (string Checker in CompileEnvironment.StaticAnalyzerAdditionalCheckers)
				{
					Arguments.Add($"-Xclang -analyzer-checker -Xclang {Checker}");
				}
			}
		}

		/// <summary>
		/// Common compile arguments for all files in a module.
		/// Override and call base.GetCompileArguments_Global() in derived classes.
		/// </summary>
		///
		/// <param name="CompileEnvironment"></param>
		/// <param name="Arguments"></param>
		protected virtual void GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			// build up the commandline common to C and C++
			Arguments.Add("-c");
			Arguments.Add("-pipe");

			// Add include paths to the argument list.
			GetCompileArguments_IncludePaths(CompileEnvironment, Arguments);

			// Add force include paths to the argument list.
			GetCompileArguments_ForceInclude(CompileEnvironment, Arguments);

			// Add preprocessor definitions to the argument list.
			GetCompileArguments_PreprocessorDefinitions(CompileEnvironment, Arguments);

			// Add warning and error flags to the argument list.
			GetCompileArguments_WarningsAndErrors(CompileEnvironment, Arguments);

			// Add optimization flags to the argument list.
			GetCompileArguments_Optimizations(CompileEnvironment, Arguments);

			// Add debugging flags to the argument list.
			GetCompileArguments_Debugging(CompileEnvironment, Arguments);

			// Add sanitizer flags to the argument list.
			GetCompilerArguments_Sanitizers(CompileEnvironment, Arguments);

			// Add analysis flags to the argument list.
			if (IsAnalyzing(CompileEnvironment))
			{
				GetCompileArguments_Analyze(CompileEnvironment, Arguments);
			}

			// Add additional arguments to the argument list.
			GetCompileArguments_AdditionalArgs(CompileEnvironment, Arguments);
		}

		/// <summary>
		/// Compile arguments for specific files in a module. Also updates Action and CPPOutput results.
		/// </summary>
		/// <param name="CompileEnvironment"></param>
		/// <param name="SourceFile"></param>
		/// <param name="OutputDir"></param>
		/// <param name="Arguments"></param>
		/// <param name="CompileAction"></param>
		/// <param name="CompileResult"></param>
		protected virtual void GetCompileArguments_FileType(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, List<string> Arguments, Action CompileAction, CPPOutput CompileResult)
		{
			// Add the C++ source file and its included files to the prerequisite item list.
			CompileAction.PrerequisiteItems.AddRange(CompileEnvironment.ForceIncludeFiles);
			CompileAction.PrerequisiteItems.AddRange(CompileEnvironment.AdditionalPrerequisites);
			CompileAction.PrerequisiteItems.Add(SourceFile);

			List<FileItem>? InlinedFiles;
			if (CompileEnvironment.FileInlineGenCPPMap.TryGetValue(SourceFile, out InlinedFiles))
			{
				CompileAction.PrerequisiteItems.AddRange(InlinedFiles);
			}

			string Extension = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant();
			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
			{
				GetCompileArguments_PCH(CompileEnvironment, Arguments);
			}
			else if (Extension == ".C")
			{
				// Compile the file as C code.
				GetCompileArguments_C(CompileEnvironment, Arguments);
			}
			else if (Extension == ".MM")
			{
				// Compile the file as Objective-C++ code.
				GetCompileArguments_MM(CompileEnvironment, Arguments);
			}
			else if (Extension == ".M")
			{
				// Compile the file as Objective-C code.
				GetCompileArguments_M(CompileEnvironment, Arguments);
			}
			else
			{
				// Compile the file as C++ code.
				GetCompileArguments_CPP(CompileEnvironment, Arguments);
			}

			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile!);
				CompileAction.PrerequisiteItems.Add(FileItem.GetItemByFileReference(CompileEnvironment.PrecompiledHeaderIncludeFilename!));
			}

			FileItem OutputFile;
			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
			{
				OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".gch"));
				CompileResult.PrecompiledHeaderFile = OutputFile;
			}
			else if (CompileEnvironment.bPreprocessOnly)
			{
				OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".i"));
				CompileResult.ObjectFiles.Add(OutputFile);

				// Clang does EITHER pre-process or object file.
				Arguments.Add("-E"); // Only run the preprocessor
				Arguments.Add("-fuse-line-directives"); // Use #line in preprocessed output

				// this is parsed by external tools wishing to open this file directly.
				Logger.LogInformation("PreProcessPath: {File}", OutputFile.AbsolutePath);
			}
			else
			{
				string ObjectFileExtension = (CompileEnvironment.AdditionalArguments != null && CompileEnvironment.AdditionalArguments.Contains("-emit-llvm")) ? ".bc" : ".o";
				OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, $"{Path.GetFileName(SourceFile.AbsolutePath)}{ObjectFileExtension}"));
				CompileResult.ObjectFiles.Add(OutputFile);
			}

			// Add the output file to the produced item list.
			CompileAction.ProducedItems.Add(OutputFile);

			// Add the source file path to the command-line.
			Arguments.Add(GetSourceFileArgument(SourceFile));

			if (!IsAnalyzing(CompileEnvironment))
			{
				// Generate the timing info
				if (CompileEnvironment.bPrintTimingInfo)
				{
					FileItem TraceFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".json"));
					Arguments.Add("-ftime-trace");
					CompileAction.ProducedItems.Add(TraceFile);
				}

				// Generate the included header dependency list
				if (!PreprocessDepends && CompileEnvironment.bGenerateDependenciesFile)
				{
					FileItem DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".d"));
					Arguments.Add(GetDepencenciesListFileArgument(DependencyListFile));
					CompileAction.DependencyListFile = DependencyListFile;
					CompileAction.ProducedItems.Add(DependencyListFile);
				}
			}
			else
			{
				OutputFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".analysis"));
			}

			// Add the parameters needed to compile the output file to the command-line.
			Arguments.Add(GetOutputFileArgument(OutputFile));
		}

		protected virtual Action CompileCPPFile(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph, IReadOnlyCollection<string> GlobalArguments, CPPOutput Result)
		{
			Action CompileAction = Graph.CreateAction(ActionType.Compile);

			List<string> FileArguments = new();

			// Add C or C++ specific compiler arguments.
			GetCompileArguments_FileType(CompileEnvironment, SourceFile, OutputDir, FileArguments, CompileAction, Result);

			// Gets the target file so we can get the correct output path.
			FileItem TargetFile = CompileAction.ProducedItems.First();

			// Creates the path to the response file using the name of the output file and creates its contents.
			FileReference ResponseFileName = new FileReference(TargetFile.AbsolutePath + ".response");
			List<string> ResponseFileContents = new();
			ResponseFileContents.AddRange(GlobalArguments);
			ResponseFileContents.AddRange(FileArguments);
			ResponseFileContents = ResponseFileContents.Select(x => Utils.ExpandVariables(x)).ToList();

			if (RuntimePlatform.IsWindows)
			{
				// Enables Clang Cmd Line Work Around
				// Clang reads the response file and computes the total cmd line length.
				// Depending on the length it will then use either cmd line or response file mode.
				// Clang currently underestimates the total size of the cmd line, which can trigger the following clang error in cmd line mode:
				// >>>> xxx-clang.exe': The filename or extension is too long.  (0xCE)
				// Clang processes and modifies the response file contents and this makes the final cmd line size hard for us to predict.
				// To be conservative we add a dummy define to inflate the response file size and force clang to use the response file mode when we are close to the limit.
				int CmdLineLength = Info.Clang.ToString().Length + string.Join(' ', ResponseFileContents).Length;
				bool bIsInDangerZone = CmdLineLength >= ClangCmdlineDangerZone && CmdLineLength <= ClangCmdLineMaxSize;
				if (bIsInDangerZone)
				{
					ResponseFileContents.Add(ClangDummyDefine);
				}
			}

			// Adds the response file to the compiler input.
			FileItem CompilerResponseFileItem = Graph.CreateIntermediateTextFile(ResponseFileName, ResponseFileContents);
			CompileAction.CommandArguments = GetResponseFileArgument(CompilerResponseFileItem);
			CompileAction.PrerequisiteItems.Add(CompilerResponseFileItem);

			CompileAction.WorkingDirectory = Unreal.EngineSourceDirectory;
			CompileAction.CommandPath = Info.Clang;
			CompileAction.CommandVersion = Info.ClangVersionString;
			CompileAction.CommandDescription = IsAnalyzing(CompileEnvironment) ? "Analyze" : "Compile";
			CompileAction.StatusDescription = Path.GetFileName(SourceFile.AbsolutePath);
			CompileAction.bIsGCCCompiler = true;

			// Don't farm out creation of pre-compiled headers as it is the critical path task.
			CompileAction.bCanExecuteRemotely =
				CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create ||
				CompileEnvironment.bAllowRemotelyCompiledPCHs;

			// Two-pass compile where the preprocessor is run first to output the dependency list
			if (PreprocessDepends && CompileEnvironment.bGenerateDependenciesFile)
			{
				Action PrepassAction = Graph.CreateAction(ActionType.Compile);
				PrepassAction.PrerequisiteItems.AddRange(CompileAction.PrerequisiteItems);
				PrepassAction.PrerequisiteItems.Remove(CompilerResponseFileItem);
				PrepassAction.CommandDescription = "Preprocess Depends";
				PrepassAction.StatusDescription = CompileAction.StatusDescription;
				PrepassAction.bIsGCCCompiler = true;
				PrepassAction.bCanExecuteRemotely = false;
				PrepassAction.bShouldOutputStatusDescription = true;
				PrepassAction.CommandPath = CompileAction.CommandPath;
				PrepassAction.CommandVersion = CompileAction.CommandVersion;
				PrepassAction.WorkingDirectory = CompileAction.WorkingDirectory;

				List<string> PreprocessGlobalArguments = new(GlobalArguments);
				List<string> PreprocessFileArguments = new(FileArguments);
				PreprocessGlobalArguments.Remove("-c");

				FileItem DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(SourceFile.AbsolutePath) + ".d"));
				PreprocessFileArguments.Add(GetPreprocessDepencenciesListFileArgument(DependencyListFile));
				PrepassAction.DependencyListFile = DependencyListFile;
				PrepassAction.ProducedItems.Add(DependencyListFile);

				PreprocessFileArguments.Remove("-ftime-trace");
				PreprocessFileArguments.Remove(GetOutputFileArgument(CompileAction.ProducedItems.First()));

				PrepassAction.DeleteItems.AddRange(PrepassAction.ProducedItems);

				// Gets the target file so we can get the correct output path.
				FileItem PreprocessTargetFile = PrepassAction.ProducedItems[0];

				// Creates the path to the response file using the name of the output file and creates its contents.
				FileReference PreprocessResponseFileName = new FileReference(PreprocessTargetFile.AbsolutePath + ".response");
				List<string> PreprocessResponseFileContents = new();
				PreprocessResponseFileContents.AddRange(PreprocessGlobalArguments);
				PreprocessResponseFileContents.AddRange(PreprocessFileArguments);

				if (RuntimePlatform.IsWindows)
				{
					int CmdLineLength = Info.Clang.ToString().Length + string.Join(' ', ResponseFileContents).Length;
					bool bIsInDangerZone = CmdLineLength >= ClangCmdlineDangerZone && CmdLineLength <= ClangCmdLineMaxSize;
					if (bIsInDangerZone)
					{
						PreprocessResponseFileContents.Add(ClangDummyDefine);
					}
				}

				// Adds the response file to the compiler input.
				FileItem PreprocessResponseFileItem = Graph.CreateIntermediateTextFile(PreprocessResponseFileName, PreprocessResponseFileContents);
				PrepassAction.PrerequisiteItems.Add(PreprocessResponseFileItem);

				PrepassAction.CommandArguments = GetResponseFileArgument(PreprocessResponseFileItem);
				CompileAction.DependencyListFile = DependencyListFile;
				CompileAction.PrerequisiteItems.Add(DependencyListFile);
			}

			return CompileAction;
		}

		public override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph)
		{
			List<string> GlobalArguments = new();
			GetCompileArguments_Global(CompileEnvironment, GlobalArguments);

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in InputFiles)
			{
				CompileCPPFile(CompileEnvironment, SourceFile, OutputDir, ModuleName, Graph, GlobalArguments, Result);
			}

			return Result;
		}

		/// <summary>
		/// Used by other tools to get the extra arguments to run vanilla clang for a particular platform.
		/// </summary>
		/// <param name="ExtraArguments">List of extra arguments to add to.</param>
		public virtual void AddExtraToolArguments(IList<string> ExtraArguments)
		{
		}
	}
}