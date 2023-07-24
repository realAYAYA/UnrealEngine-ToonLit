// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	class IWYUToolChain : ClangToolChain
	{
		private List<string> CrossCompilingArguments = new();
		private FileReference IWYUMappingFile;
		private string RelativePathToIWYUDirectory = @"Restricted\NotForLicensees\Source\ThirdParty\IWYU";

		public static void ValidateTarget(TargetRules Target)
		{
			Target.bDisableDebugInfo = true;
			Target.bDisableLinking = true;
			Target.bUsePCHFiles = false;
			Target.bUseSharedPCHs = false;
			Target.bUseUnityBuild = false;
			Target.bCompileISPC = false;
		}

		public IWYUToolChain(ReadOnlyTargetRules Target, ILogger InLogger) : base(ClangToolChainOptions.None, InLogger)
		{
			// set up the path to our toolchains
			IWYUMappingFile = FileReference.Combine(Unreal.EngineDirectory, RelativePathToIWYUDirectory, "ue_mapping.imp");
			if (IWYUMappingFile == null)
			{
				throw new BuildException("It seems you don't have access to NotForLicensees folder.\n" +
										 "IWYU is not yet released to the public." +
										 "We are working on validating so we can release the modified iwyu exe");
			}
		}

		protected override ClangToolChainInfo GetToolChainInfo()
		{
			var PlatformSDK = new LinuxPlatformSDK(Logger);
			DirectoryReference? BaseLinuxPath = PlatformSDK.GetBaseLinuxPathForArchitecture(LinuxPlatform.DefaultHostArchitecture);

			CrossCompilingArguments.Add($"--sysroot=\"{NormalizeCommandLinePath(BaseLinuxPath!)}\"");

			FileReference ClangPath = FileReference.Combine(BaseLinuxPath!, "bin", $"clang++{BuildHostPlatform.Current.BinarySuffix}");
			ClangToolChainInfo CompilerToolChainInfo = new ClangToolChainInfo(ClangPath, null!, Logger);

			DirectoryReference SystemPath = DirectoryReference.Combine(BaseLinuxPath!, "lib", "clang", CompilerToolChainInfo.ClangVersion.ToString(), "include");
			CrossCompilingArguments.Add(GetSystemIncludePathArgument(SystemPath));

			string DevPath = ""; //@"include-what-you-use-0.19\vs_projects\bin\RelWithDebInfo";

			FileReference IWYUPath = FileReference.Combine(Unreal.EngineDirectory, RelativePathToIWYUDirectory, DevPath, @"include-what-you-use.exe");
			return new ClangToolChainInfo(IWYUPath!, null!, Logger);
		}

		protected override string GetFileNameFromExtension(string AbsolutePath, string Extension)
		{
			string FileName = Path.GetFileName(AbsolutePath);
			if (AbsolutePath.EndsWith(".h"))
			{
				string HashString = ContentHash.MD5(AbsolutePath).GetHashCode().ToString("X4");
				FileName = Path.GetFileNameWithoutExtension(FileName) + "_" + HashString + ".h";

				if (Extension != ".d" && Extension != ".o")
				{
					throw new NotImplementedException($"Files with extension {Extension} not handled by IWYUToolChain");
				}
			}

			if (Extension == ".o")
			{
				Extension = ".iwyu";
			}

			return FileName + Extension;
		}

		protected override CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph)
		{
			if (InputFiles.Count == 0)
			{
				return new CPPOutput();
			}

			Dictionary<FileItem, List<FileItem>> GenCppLookup = CompileEnvironment.FileInlineGenCPPMap;

			CompileEnvironment = new(CompileEnvironment);
			CompileEnvironment.Definitions.Add("SUPPRESS_MONOLITHIC_HEADER_WARNINGS=1");
			CompileEnvironment.Definitions.Add("PLATFORM_COMPILER_IWYU=1");

			// Remove this once c++20 linux compile errors are fixed
			CompileEnvironment.CppStandard = CppStandardVersion.Cpp17;

			List<string> GlobalArguments = new();
			GetCompileArguments_Global(CompileEnvironment, GlobalArguments);

			// Need to add these on cmd line.. IWYU is not parsing commands after response file expansion.
			string CommonCommandLineArgs = " -Xiwyu --mapping_file=" + IWYUMappingFile.FullName +
										   " -Xiwyu --prefix_header_includes=keep" +
//										   " -Xiwyu --transitive_includes_only" +  // Since we are building headers separately (not together with their cpp) we don't need this
										   " -Xiwyu --no_check_matching_header" +
										   " -Xiwyu --cxx17ns";

			// Create a compile action for each source file.
			List<FileItem> IwyuFiles = new List<FileItem>();
			foreach (FileItem SourceFile in InputFiles)
			{
				//if (SourceFile.HasExtension(".cpp") || !SourceFile.FullName.Contains("\\Runtime\\Engine\\"))
				//	continue;

				Action Action = CompileCPPFile(CompileEnvironment, SourceFile, OutputDir, ModuleName, Graph, GlobalArguments, new CPPOutput());

				string CommandLineArgs = CommonCommandLineArgs + " -Xiwyu --write_json_path=\"" + Action.ProducedItems.First() + "\" ";
				if (SourceFile.HasExtension(".cpp"))
				{
					List<FileItem>? InlinedFiles;
					if (GenCppLookup.TryGetValue(SourceFile, out InlinedFiles))
					{
						foreach (FileItem InlinedFile in InlinedFiles)
						{
							CommandLineArgs += $"-Xiwyu --check_also={InlinedFile.FullName.Replace('\\', '/')} ";
						}
					}
				}

				Action.CommandArguments = CommandLineArgs + Action.CommandArguments;
				IwyuFiles.Add(Action.ProducedItems.First());
			}

			return new CPPOutput() { ObjectFiles = IwyuFiles };
		}

		// Include paths must be absolute because we have code gen that can produce includes that looks like this: "../../../Source/Foo/Bar.h"
		// IWYU combines include path and include and then try to normalize the result. This could result in a path that can't be normalized because there are more
		// ".." than there are directories to reduce. This silently fails in odd ways

		protected override string NormalizeCommandLinePath(FileSystemReference Reference)
		{
			return Reference.FullName.Replace('\\', '/');
		}

		protected override string NormalizeCommandLinePath(FileItem Item)
		{
			return Item.FullName.Replace('\\', '/');
		}

		// Code below here is mostly copy-pasted from LinuxToolChain.
		// We didn't inherit from LinuxToolChain on purpose in order to be able to add support for running iwyu against other clang platforms than linux

		private static bool ShouldUseLibcxx(UnrealArch Architecture)
		{
			// set UE_LINUX_USE_LIBCXX to either 0 or 1. If unset, defaults to 1.
			string? UseLibcxxEnvVarOverride = Environment.GetEnvironmentVariable("UE_LINUX_USE_LIBCXX");
			if (string.IsNullOrEmpty(UseLibcxxEnvVarOverride) || UseLibcxxEnvVarOverride == "1")
			{
				return true;
			}
			return false;
		}

		protected override void GetCompileArguments_Global(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			base.GetCompileArguments_Global(CompileEnvironment, Arguments);

			if (ShouldUseLibcxx(CompileEnvironment.Architecture))
			{
				Arguments.Add("-nostdinc++");
				Arguments.Add(GetSystemIncludePathArgument(DirectoryReference.Combine(Unreal.EngineSourceDirectory, "ThirdParty", "Unix", "LibCxx", "include")));
				Arguments.Add(GetSystemIncludePathArgument(DirectoryReference.Combine(Unreal.EngineSourceDirectory, "ThirdParty", "Unix", "LibCxx", "include", "c++", "v1")));
			}

			Arguments.Add("-fno-math-errno");               // do not assume that math ops have side effects

			Arguments.Add(GetRTTIFlag(CompileEnvironment)); // flag for run-time type info

			if (true)//CrossCompiling())
			{
				Arguments.Add($"-target {CompileEnvironment.Architecture.LinuxName}");        // Set target triple
				Arguments.AddRange(CrossCompilingArguments);
			}

			if (CompileEnvironment.bHideSymbolsByDefault)
			{
				Arguments.Add("-fvisibility-ms-compat");
				Arguments.Add("-fvisibility-inlines-hidden");
			}
		}

		protected override void GetCompileArguments_FileType(CppCompileEnvironment CompileEnvironment, FileItem SourceFile, DirectoryReference OutputDir, List<string> Arguments, Action CompileAction, CPPOutput CompileResult)
		{
			base.GetCompileArguments_FileType(CompileEnvironment, SourceFile, OutputDir, Arguments, CompileAction, CompileResult);
		}

		protected override void GetCompileArguments_WarningsAndErrors(CppCompileEnvironment CompileEnvironment, List<string> Arguments)
		{
			CompileEnvironment.ShadowVariableWarningLevel = WarningLevel.Off;
			base.GetCompileArguments_WarningsAndErrors(CompileEnvironment, Arguments);
			Arguments.Add("-Wno-undefined-bool-conversion");
			Arguments.Add("-Wno-deprecated-anon-enum-enum-conversion");
			Arguments.Add("-Wno-ambiguous-reversed-operator");
			Arguments.Add("-Wno-pragma-once-outside-header");
		}

		// Skip ISPC headers
		public override CPPOutput GenerateISPCHeaders(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, IActionGraphBuilder Graph)
		{
			return new CPPOutput();
		}
		public override CPPOutput CompileISPCFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, IActionGraphBuilder Graph)
		{
			return new CPPOutput();
		}

		// IWYU can't link
		public override FileItem LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			throw new BuildException("Unable to link with IWYU toolchain.");
		}
	}
}