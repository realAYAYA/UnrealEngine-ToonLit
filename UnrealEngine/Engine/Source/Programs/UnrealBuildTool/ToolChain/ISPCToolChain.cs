// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	abstract class ISPCToolChain : UEToolChain
	{
		public ISPCToolChain(ILogger InLogger) : base(InLogger)
		{
		}

		/// <summary>
		/// Get CPU Instruction set targets for ISPC.
		/// </summary>
		/// <param name="Platform">Which OS platform to target.</param>
		/// <param name="Arch">Which architecture inside an OS platform to target. Only used for Android currently.</param>
		/// <returns>List of instruction set targets passed to ISPC compiler</returns>
		public virtual List<string> GetISPCCompileTargets(UnrealTargetPlatform Platform, UnrealArch? Arch)
		{
			List<string> ISPCTargets = new List<string>();

			// @todo this could be simplified for the arm case - but sse has more options
			if (UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Windows) ||
				(UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Unix) && Platform != UnrealTargetPlatform.LinuxArm64) ||
				Platform == UnrealTargetPlatform.Mac)
			{
				ISPCTargets.AddRange(new string[] { "avx512skx-i32x8", "avx2", "avx", "sse4" });
			}
			else if (Platform == UnrealTargetPlatform.LinuxArm64)
			{
				ISPCTargets.AddRange(new string[] { "neon" });
			}
			else if (Platform == UnrealTargetPlatform.Android)
			{
				if (Arch == UnrealArch.X64)
				{
					ISPCTargets.Add("sse4");
				}
				else if (Arch == UnrealArch.Arm64)
				{
					ISPCTargets.Add("neon");
				}
				else
				{
					Logger.LogWarning("Invalid Android architecture for ISPC. At least one architecture (arm64, x64) needs to be selected in the project settings to build");
				}
			}
			else if (Platform == UnrealTargetPlatform.IOS)
			{
				ISPCTargets.Add("neon");
			}
			else
			{
				Logger.LogWarning("Unsupported ISPC platform target!");
			}

			return ISPCTargets;
		}

		/// <summary>
		/// Get OS target for ISPC.
		/// </summary>
		/// <param name="Platform">Which OS platform to target.</param>
		/// <returns>OS string passed to ISPC compiler</returns>
		public virtual string GetISPCOSTarget(UnrealTargetPlatform Platform)
		{
			string ISPCOS = "";

			if (UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Windows))
			{
				ISPCOS += "windows";
			}
			else if (UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Unix))
			{
				ISPCOS += "linux";
			}
			else if (Platform == UnrealTargetPlatform.Android)
			{
				ISPCOS += "android";
			}
			else if (Platform == UnrealTargetPlatform.IOS)
			{
				ISPCOS += "ios";
			}
			else if (Platform == UnrealTargetPlatform.Mac)
			{
				ISPCOS += "macos";
			}
			else
			{
				Logger.LogWarning("Unsupported ISPC platform target!");
			}

			return ISPCOS;
		}

		/// <summary>
		/// Get CPU architecture target for ISPC.
		/// </summary>
		/// <param name="Platform">Which OS platform to target.</param>
		/// <param name="Arch">Which architecture inside an OS platform to target. Only used for Android currently.</param>
		/// <returns>Arch string passed to ISPC compiler</returns>
		public virtual string GetISPCArchTarget(UnrealTargetPlatform Platform, UnrealArch? Arch)
		{
			string ISPCArch = "";

			if (UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Windows) ||
				(UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Unix) && Platform != UnrealTargetPlatform.LinuxArm64) ||
				Platform == UnrealTargetPlatform.Mac)
			{
				ISPCArch += "x86-64";
			}
			else if (Platform == UnrealTargetPlatform.LinuxArm64)
			{
				ISPCArch += "aarch64";
			}
			else if (Platform == UnrealTargetPlatform.Android)
			{
				if (Arch == UnrealArch.Arm64)
				{
					ISPCArch += "aarch64";
				}
				else if (Arch == UnrealArch.X64)
				{
					ISPCArch += "x86-64";
				}
				else
				{
					Logger.LogWarning("Invalid Android architecture for ISPC. At least one architecture (arm64, x64) needs to be selected in the project settings to build");
				}
			}
			else if (Platform == UnrealTargetPlatform.IOS)
			{
				ISPCArch += "aarch64";
			}
			else
			{
				Logger.LogWarning("Unsupported ISPC platform target!");
			}

			return ISPCArch;
		}

		/// <summary>
		/// Get CPU target for ISPC.
		/// </summary>
		/// <param name="Platform">Which OS platform to target.</param>
		/// <returns>CPU string passed to ISPC compiler</returns>
		public virtual string? GetISPCCpuTarget(UnrealTargetPlatform Platform)
		{
			return null;  // no specific CPU selected
		}

		/// <summary>
		/// Get host compiler path for ISPC.
		/// </summary>
		/// <param name="HostPlatform">Which OS build platform is running on.</param>
		/// <returns>Path to ISPC compiler</returns>
		public virtual string GetISPCHostCompilerPath(UnrealTargetPlatform HostPlatform)
		{
			string ISPCCompilerPathCommon = Path.Combine(Unreal.EngineSourceDirectory.FullName, "ThirdParty", "Intel", "ISPC", "bin");
			string ISPCArchitecturePath = "";
			string ExeExtension = ".exe";

			if (UEBuildPlatform.IsPlatformInGroup(HostPlatform, UnrealPlatformGroup.Windows))
			{
				ISPCArchitecturePath = "Windows";
			}
			else if (HostPlatform == UnrealTargetPlatform.Linux)
			{
				ISPCArchitecturePath = "Linux";
				ExeExtension = "";
			}
			else if (HostPlatform == UnrealTargetPlatform.Mac)
			{
				ISPCArchitecturePath = "Mac";
				ExeExtension = "";
			}
			else
			{
				Logger.LogWarning("Unsupported ISPC host!");
			}

			return Path.Combine(ISPCCompilerPathCommon, ISPCArchitecturePath, "ispc" + ExeExtension);
		}

		/// <summary>
		/// Get the host bytecode-to-obj compiler path for ISPC. Only used for platforms that support compiling ISPC to LLVM bytecode
		/// </summary>
		/// <param name="HostPlatform">Which OS build platform is running on.</param>
		/// <returns>Path to bytecode to obj compiler</returns>
		public virtual string? GetISPCHostBytecodeCompilerPath(UnrealTargetPlatform HostPlatform)
		{
			// Return null if the platform toolchain doesn't support separate bytecode to obj compilation
			return null;
		}

		static Dictionary<UnrealTargetPlatform, string> ISPCCompilerVersions = new Dictionary<UnrealTargetPlatform, string>();

		/// <summary>
		/// Returns the version of the ISPC compiler for the specified platform. If GetISPCHostCompilerPath() doesn't return a valid path
		/// this will return a -1 version.
		/// </summary>
		/// <param name="Platform">Which OS build platform is running on.</param>
		/// <returns>Version reported by the ISPC compilerr</returns>
		public virtual string GetISPCHostCompilerVersion(UnrealTargetPlatform Platform)
		{
			if (!ISPCCompilerVersions.ContainsKey(Platform))
			{
				Version? CompilerVersion = null;
				string CompilerPath = GetISPCHostCompilerPath(Platform);

				if (!File.Exists(CompilerPath))
				{
					Logger.LogWarning("No ISPC compiler at {CompilerPath}", CompilerPath);
					CompilerVersion = new Version(-1, -1);
				}

				ISPCCompilerVersions[Platform] = RunToolAndCaptureOutput(new FileReference(CompilerPath), "--version", "(.*)")!;
			}

			return ISPCCompilerVersions[Platform];
		}

		/// <summary>
		/// Get object file format for ISPC.
		/// </summary>
		/// <param name="Platform">Which OS build platform is running on.</param>
		/// <returns>Object file suffix</returns>
		public virtual string GetISPCObjectFileFormat(UnrealTargetPlatform Platform)
		{
			string Format = "";

			if (UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Windows))
			{
				Format += "obj";
			}
			else if (UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Unix) ||
					Platform == UnrealTargetPlatform.Mac ||
					Platform == UnrealTargetPlatform.IOS ||
					Platform == UnrealTargetPlatform.Android)
			{
				Format += "obj";
			}
			else
			{
				Logger.LogWarning("Unsupported ISPC platform target!");
			}

			return Format;
		}

		/// <summary>
		/// Get object file suffix for ISPC.
		/// </summary>
		/// <param name="Platform">Which OS build platform is running on.</param>
		/// <returns>Object file suffix</returns>
		public virtual string GetISPCObjectFileSuffix(UnrealTargetPlatform Platform)
		{
			string Suffix = "";

			if (UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Windows))
			{
				Suffix += ".obj";
			}
			else if (UEBuildPlatform.IsPlatformInGroup(Platform, UnrealPlatformGroup.Unix) ||
					Platform == UnrealTargetPlatform.Mac ||
					Platform == UnrealTargetPlatform.IOS ||
					Platform == UnrealTargetPlatform.Android)
			{
				Suffix += ".o";
			}
			else
			{
				Logger.LogWarning("Unsupported ISPC platform target!");
			}

			return Suffix;
		}

		private string EscapeDefinitionForISPC(string Definition)
		{
			// See: https://github.com/ispc/ispc/blob/4ee767560cd752eaf464c124eb7ef1b0fd37f1df/src/main.cpp#L264 for ispc's argument parsing code, which does the following (and does not support escaping):
			// Argument      Parses as 
			// "abc""def"    One agrument:  abcdef
			// "'abc'"       One argument:  'abc'
			// -D"X="Y Z""   Two arguments: -DX=Y and Z
			// -D'X="Y Z"'   One argument:  -DX="Y Z"  (i.e. with quotes in value)
			// -DX="Y Z"     One argument:  -DX=Y Z    (this is what we want on the command line)

			// Assumes that quotes at the start and end of the value string mean that everything between them should be passed on unchanged.

			int DoubleQuoteCount = Definition.Count(c => c == '"');
			bool bHasSingleQuote = Definition.Contains('\'');
			bool bHasSpace = Definition.Contains(' ');

			string Escaped = Definition;

			if (DoubleQuoteCount > 0 || bHasSingleQuote || bHasSpace)
			{
				int EqualsIndex = Definition.IndexOf('=');
				string Name = Definition[0..EqualsIndex];
				string Value = Definition[(EqualsIndex + 1)..];

				string UnquotedValue = Value;

				// remove one layer of quoting, if present
				if (Value.StartsWith('"') && Value.EndsWith('"') && Value.Length != 1)
				{
					UnquotedValue = Value[1..^1];
					DoubleQuoteCount -= 2;
				}

				if (DoubleQuoteCount == 0 && (bHasSingleQuote || bHasSpace))
				{
					Escaped = $"{Name}=\"{UnquotedValue}\"";
				}
				else if (!bHasSingleQuote && (bHasSpace || DoubleQuoteCount > 0))
				{
					// If there are no single quotes, we can use them to quote the value string
					Escaped = $"{Name}='{UnquotedValue}'";
				}
				else
				{
					// Treat all special chars in the value string as needing explicit extra quoting. Thoroughly clumsy.
					StringBuilder Requoted = new StringBuilder();
					foreach (char c in UnquotedValue)
					{
						if (c == '"')
						{
							Requoted.Append("'\"'");
						}
						else if (c == '\'')
						{
							Requoted.Append("\"'\"");
						}
						else if (c == ' ')
						{
							Requoted.Append("\" \"");
						}
						else
						{
							Requoted.Append(c);
						}
					}
					Escaped = $"{Name}={Requoted}";
				}
			}

			return Escaped;
		}

		/// <summary>
		/// Normalize a path for use in a command line, making it relative to Engine/Source if under the root directory
		/// </summary>
		/// <param name="Reference">The FileSystemReference to normalize</param>
		/// <returns>Normalized path as a string</returns>
		protected virtual string NormalizeCommandLinePath(FileSystemReference Reference)
		{
			// Try to use a relative path to shorten command line length.
			if (Reference.IsUnderDirectory(Unreal.RootDirectory))
			{
				return Reference.MakeRelativeTo(Unreal.EngineSourceDirectory).Replace("\\", "/");
			}

			return Reference.FullName.Replace("\\", "/");
		}

		/// <summary>
		/// Normalize a path for use in a command line, making it relative if under the Root Directory
		/// </summary>
		/// <param name="Item">The FileItem to normalize</param>
		/// <returns>Normalized path as a string</returns>
		protected virtual string NormalizeCommandLinePath(FileItem Item)
		{
			return NormalizeCommandLinePath(Item.Location);
		}

		public override CPPOutput GenerateISPCHeaders(CppCompileEnvironment CompileEnvironment, IEnumerable<FileItem> InputFiles, DirectoryReference OutputDir, IActionGraphBuilder Graph)
		{
			CPPOutput Result = new CPPOutput();

			if (!CompileEnvironment.bCompileISPC)
			{
				return Result;
			}

			List<string> CompileTargets = GetISPCCompileTargets(CompileEnvironment.Platform, null);

			List<string> GlobalArguments = new List<string>();

			// Build target string. No comma on last
			string TargetString = "";
			foreach (string Target in CompileTargets)
			{
				if (Target == CompileTargets[CompileTargets.Count - 1]) // .Last()
				{
					TargetString += Target;
				}
				else
				{
					TargetString += Target + ",";
				}
			}

			string ISPCArch = GetISPCArchTarget(CompileEnvironment.Platform, null);

			// Build target triplet
			GlobalArguments.Add($"--target-os={GetISPCOSTarget(CompileEnvironment.Platform)}");
			GlobalArguments.Add($"--arch={ISPCArch}");
			GlobalArguments.Add($"--target={TargetString}");
			GlobalArguments.Add($"--emit-{GetISPCObjectFileFormat(CompileEnvironment.Platform)}");

			string? CpuTarget = GetISPCCpuTarget(CompileEnvironment.Platform);
			if (!String.IsNullOrEmpty(CpuTarget))
			{
				GlobalArguments.Add($"--cpu={CpuTarget}");
			}

			// PIC is needed for modular builds except on Microsoft platforms
			if ((CompileEnvironment.bIsBuildingDLL ||
				CompileEnvironment.bIsBuildingLibrary) &&
				!UEBuildPlatform.IsPlatformInGroup(CompileEnvironment.Platform, UnrealPlatformGroup.Microsoft))
			{
				GlobalArguments.Add("--pic");
			}

			// Include paths. Don't use AddIncludePath() here, since it uses the full path and exceeds the max command line length.
			// Because ISPC response files don't support white space in arguments, paths with white space need to be passed to the command line directly.
			foreach (DirectoryReference IncludePath in CompileEnvironment.UserIncludePaths)
			{
				GlobalArguments.Add($"-I\"{NormalizeCommandLinePath(IncludePath)}\"");
			}

			// System include paths.
			foreach (DirectoryReference SystemIncludePath in CompileEnvironment.SystemIncludePaths)
			{
				GlobalArguments.Add($"-I\"{NormalizeCommandLinePath(SystemIncludePath)}\"");
			}

			// Preprocessor definitions.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				// TODO: Causes ISPC compiler to generate a spurious warning about the universal character set
				if (!Definition.Contains("\\\\U") && !Definition.Contains("\\\\u"))
				{
					GlobalArguments.Add($"-D{EscapeDefinitionForISPC(Definition)}");
				}
			}

			foreach (FileItem ISPCFile in InputFiles)
			{
				Action CompileAction = Graph.CreateAction(ActionType.Compile);
				CompileAction.CommandDescription = $"Generate Header [{ISPCArch}]";
				CompileAction.WorkingDirectory = Unreal.EngineSourceDirectory;
				CompileAction.CommandPath = new FileReference(GetISPCHostCompilerPath(BuildHostPlatform.Current.Platform));
				CompileAction.StatusDescription = Path.GetFileName(ISPCFile.AbsolutePath);
				CompileAction.ArtifactMode = ArtifactMode.Enabled;

				CompileAction.bCanExecuteRemotely = true;

				// Disable remote execution to workaround mismatched case on XGE
				CompileAction.bCanExecuteRemotelyWithXGE = false;

				// TODO: Remove, might work
				CompileAction.bCanExecuteRemotelyWithSNDBS = false;

				List<string> Arguments = new List<string>();

				// Add the ISPC obj file as a prerequisite of the action.
				Arguments.Add($"\"{NormalizeCommandLinePath(ISPCFile)}\"");

				// Add the ISPC h file to the produced item list.
				FileItem ISPCIncludeHeaderFile = FileItem.GetItemByFileReference(
					FileReference.Combine(
						OutputDir,
						Path.GetFileName(ISPCFile.AbsolutePath) + ".generated.dummy.h"
						)
					);

				// Add the ISPC file to be compiled.
				Arguments.Add($"-h \"{NormalizeCommandLinePath(ISPCIncludeHeaderFile)}\"");

				// Generate the included header dependency list
				FileItem DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(ISPCFile.AbsolutePath) + ".txt"));
				Arguments.Add($"-MMM \"{NormalizeCommandLinePath(DependencyListFile)}\"");
				CompileAction.DependencyListFile = DependencyListFile;
				CompileAction.ProducedItems.Add(DependencyListFile);

				Arguments.AddRange(GlobalArguments);

				CompileAction.ProducedItems.Add(ISPCIncludeHeaderFile);

				FileReference ResponseFileName = GetResponseFileName(CompileEnvironment, ISPCIncludeHeaderFile);
				FileItem ResponseFileItem = Graph.CreateIntermediateTextFile(ResponseFileName, Arguments.Select(x => Utils.ExpandVariables(x)));
				CompileAction.CommandArguments = $"@\"{NormalizeCommandLinePath(ResponseFileName)}\"";
				CompileAction.PrerequisiteItems.Add(ResponseFileItem);

				// Add the source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(ISPCFile);

				FileItem ISPCFinalHeaderFile = FileItem.GetItemByFileReference(
					FileReference.Combine(
						OutputDir,
						Path.GetFileName(ISPCFile.AbsolutePath) + ".generated.h"
						)
					);

				// Fix interrupted build issue by copying header after generation completes
				Action CopyAction = Graph.CreateCopyAction(ISPCIncludeHeaderFile, ISPCFinalHeaderFile);
				CopyAction.CommandDescription = $"{CopyAction.CommandDescription} [{ISPCArch}]";
				CopyAction.DeleteItems.Clear();
				CopyAction.PrerequisiteItems.Add(ISPCFile);
				CopyAction.bShouldOutputStatusDescription = false;

				Result.GeneratedHeaderFiles.Add(ISPCFinalHeaderFile);

				Logger.LogDebug("   ISPC Generating Header {StatusDescription}: \"{CommandPath}\" {CommandArguments}", CompileAction.StatusDescription, CompileAction.CommandPath, CompileAction.CommandArguments);
			}

			return Result;
		}

		public override CPPOutput CompileISPCFiles(CppCompileEnvironment CompileEnvironment, IEnumerable<FileItem> InputFiles, DirectoryReference OutputDir, IActionGraphBuilder Graph)
		{
			CPPOutput Result = new CPPOutput();

			if (!CompileEnvironment.bCompileISPC)
			{
				return Result;
			}

			List<string> CompileTargets = GetISPCCompileTargets(CompileEnvironment.Platform, null);

			List<string> GlobalArguments = new List<string>();

			// Build target string. No comma on last
			string TargetString = "";
			foreach (string Target in CompileTargets)
			{
				if (Target == CompileTargets[CompileTargets.Count - 1]) // .Last()
				{
					TargetString += Target;
				}
				else
				{
					TargetString += Target + ",";
				}
			}

			// Build target triplet
			string PlatformObjectFileFormat = GetISPCObjectFileFormat(CompileEnvironment.Platform);
			string ISPCArch = GetISPCArchTarget(CompileEnvironment.Platform, null);

			GlobalArguments.Add($"--target-os={GetISPCOSTarget(CompileEnvironment.Platform)}");
			GlobalArguments.Add($"--arch={ISPCArch}");
			GlobalArguments.Add($"--target={TargetString}");
			GlobalArguments.Add($"--emit-{PlatformObjectFileFormat}");

			string? CpuTarget = GetISPCCpuTarget(CompileEnvironment.Platform);
			if (!String.IsNullOrEmpty(CpuTarget))
			{
				GlobalArguments.Add($"--cpu={CpuTarget}");
			}

			bool bByteCodeOutput = (PlatformObjectFileFormat == "llvm");

			List<string> CommonArgs = new List<string>();
			if (CompileEnvironment.Configuration == CppConfiguration.Debug)
			{
				if (CompileEnvironment.Platform == UnrealTargetPlatform.Mac)
				{
					// Turn off debug symbols on Mac due to dsym generation issue
					CommonArgs.Add("-O0");
					// Ideally we would be able to turn on symbols and specify the dwarf version, but that does
					// does not seem to be working currently, ie:
					//    GlobalArguments.Add("-g -O0 --dwarf-version=2");

				}
				else
				{
					CommonArgs.Add("-g -O0");
				}
			}
			else
			{
				CommonArgs.Add("-O3");
			}
			GlobalArguments.AddRange(CommonArgs);

			// PIC is needed for modular builds except on Microsoft platforms
			if ((CompileEnvironment.bIsBuildingDLL ||
				CompileEnvironment.bIsBuildingLibrary) &&
				!UEBuildPlatform.IsPlatformInGroup(CompileEnvironment.Platform, UnrealPlatformGroup.Microsoft))
			{
				GlobalArguments.Add("--pic");
			}

			// Include paths. Don't use AddIncludePath() here, since it uses the full path and exceeds the max command line length.
			foreach (DirectoryReference IncludePath in CompileEnvironment.UserIncludePaths)
			{
				GlobalArguments.Add($"-I\"{NormalizeCommandLinePath(IncludePath)}\"");
			}

			// System include paths.
			foreach (DirectoryReference SystemIncludePath in CompileEnvironment.SystemIncludePaths)
			{
				GlobalArguments.Add($"-I\"{NormalizeCommandLinePath(SystemIncludePath)}\"");
			}

			// Preprocessor definitions.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				// TODO: Causes ISPC compiler to generate a spurious warning about the universal character set
				if (!Definition.Contains("\\\\U") && !Definition.Contains("\\\\u"))
				{
					GlobalArguments.Add($"-D{EscapeDefinitionForISPC(Definition)}");
				}
			}

			foreach (FileItem ISPCFile in InputFiles)
			{
				Action CompileAction = Graph.CreateAction(ActionType.Compile);
				CompileAction.CommandDescription = $"Compile [{ISPCArch}]";
				CompileAction.WorkingDirectory = Unreal.EngineSourceDirectory;
				CompileAction.CommandPath = new FileReference(GetISPCHostCompilerPath(BuildHostPlatform.Current.Platform));
				CompileAction.StatusDescription = Path.GetFileName(ISPCFile.AbsolutePath);

				CompileAction.bCanExecuteRemotely = true;

				// Disable remote execution to workaround mismatched case on XGE
				CompileAction.bCanExecuteRemotelyWithXGE = false;

				// TODO: Remove, might work
				CompileAction.bCanExecuteRemotelyWithSNDBS = false;

				List<string> Arguments = new List<string>();

				// Add the ISPC file to be compiled.
				Arguments.Add($"\"{NormalizeCommandLinePath(ISPCFile)}\"");

				List<FileItem> CompiledISPCObjFiles = new List<FileItem>();

				string CompiledISPCObjFileSuffix = bByteCodeOutput ? ".bc" : GetISPCObjectFileSuffix(CompileEnvironment.Platform);
				foreach (string Target in CompileTargets)
				{
					string ObjTarget = Target;

					if (Target.Contains('-'))
					{
						// Remove lane width and gang size from obj file name
						ObjTarget = Target.Split('-')[0];
					}

					FileItem CompiledISPCObjFile;

					if (CompileTargets.Count > 1)
					{
						CompiledISPCObjFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							OutputDir,
							Path.GetFileName(ISPCFile.AbsolutePath) + "_" + ObjTarget + CompiledISPCObjFileSuffix
							)
						);
					}
					else
					{
						CompiledISPCObjFile = FileItem.GetItemByFileReference(
						FileReference.Combine(
							OutputDir,
							Path.GetFileName(ISPCFile.AbsolutePath) + CompiledISPCObjFileSuffix
							)
						);
					}

					// Add the ISA specific ISPC obj files to the produced item list.
					CompiledISPCObjFiles.Add(CompiledISPCObjFile);
				}

				// Add the common ISPC obj file to the produced item list if it's not already in it
				FileItem CompiledISPCObjFileNoISA = FileItem.GetItemByFileReference(
					FileReference.Combine(
						OutputDir,
						Path.GetFileName(ISPCFile.AbsolutePath) + CompiledISPCObjFileSuffix
						)
					);

				if (CompileTargets.Count > 1)
				{
					CompiledISPCObjFiles.Add(CompiledISPCObjFileNoISA);
				}

				// Add the output ISPC obj file
				Arguments.Add($"-o \"{NormalizeCommandLinePath(CompiledISPCObjFileNoISA)}\"");

				// Generate the timing info
				if (CompileEnvironment.bPrintTimingInfo)
				{
					FileItem TraceFile = FileItem.GetItemByFileReference(FileReference.FromString($"{CompiledISPCObjFileNoISA}.json"));
					Arguments.Add("--time-trace");
					CompileAction.ProducedItems.Add(TraceFile);
				}

				Arguments.AddRange(GlobalArguments);

				// Consume the included header dependency list
				FileItem DependencyListFile = FileItem.GetItemByFileReference(FileReference.Combine(OutputDir, Path.GetFileName(ISPCFile.AbsolutePath) + ".txt"));
				CompileAction.DependencyListFile = DependencyListFile;
				CompileAction.PrerequisiteItems.Add(DependencyListFile);

				CompileAction.ProducedItems.UnionWith(CompiledISPCObjFiles);

				FileReference ResponseFileName = GetResponseFileName(CompileEnvironment, CompiledISPCObjFileNoISA);
				FileItem ResponseFileItem = Graph.CreateIntermediateTextFile(ResponseFileName, Arguments.Select(x => Utils.ExpandVariables(x)));
				CompileAction.CommandArguments = $"@\"{ResponseFileName}\"";
				CompileAction.PrerequisiteItems.Add(ResponseFileItem);

				// Add the source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(ISPCFile);

				Logger.LogDebug("   ISPC Compiling {StatusDescription}: \"{CommandPath}\" {CommandArguments}", CompileAction.StatusDescription, CompileAction.CommandPath, CompileAction.CommandArguments);

				if (bByteCodeOutput)
				{
					// If the platform toolchain supports bytecode compilation for ISPC, compile the bytecode object files to actual native object files 
					string? ByteCodeCompilerPath = GetISPCHostBytecodeCompilerPath(BuildHostPlatform.Current.Platform);
					if (ByteCodeCompilerPath != null)
					{
						List<FileItem> FinalObjectFiles = new List<FileItem>();
						foreach (FileItem CompiledBytecodeObjFile in CompiledISPCObjFiles)
						{
							FileItem FinalCompiledISPCObjFile = FileItem.GetItemByFileReference(
								FileReference.Combine(
									OutputDir,
									Path.GetFileNameWithoutExtension(CompiledBytecodeObjFile.AbsolutePath) + GetISPCObjectFileSuffix(CompileEnvironment.Platform)
									)
								);

							Action PostCompileAction = Graph.CreateAction(ActionType.Compile);

							List<string> PostCompileArgs = new List<string>();
							PostCompileArgs.Add($"\"{NormalizeCommandLinePath(CompiledBytecodeObjFile)}\"");
							PostCompileArgs.Add("-c");
							PostCompileArgs.AddRange(CommonArgs);
							PostCompileArgs.Add($"-o \"{NormalizeCommandLinePath(FinalCompiledISPCObjFile)}\"");

							// Write the args to a response file
							FileReference PostCompileResponseFileName = GetResponseFileName(CompileEnvironment, FinalCompiledISPCObjFile);
							FileItem PostCompileResponseFileItem = Graph.CreateIntermediateTextFile(PostCompileResponseFileName, PostCompileArgs.Select(x => Utils.ExpandVariables(x)));
							PostCompileAction.CommandArguments = $"@\"{PostCompileResponseFileName}\"";
							PostCompileAction.PrerequisiteItems.Add(PostCompileResponseFileItem);

							PostCompileAction.PrerequisiteItems.Add(CompiledBytecodeObjFile);
							PostCompileAction.ProducedItems.Add(FinalCompiledISPCObjFile);
							PostCompileAction.CommandDescription = $"CompileByteCode [{ISPCArch}]";
							PostCompileAction.WorkingDirectory = Unreal.EngineSourceDirectory;
							PostCompileAction.CommandPath = new FileReference(ByteCodeCompilerPath);
							PostCompileAction.StatusDescription = Path.GetFileName(ISPCFile.AbsolutePath);

							// Disable remote execution to workaround mismatched case on XGE
							PostCompileAction.bCanExecuteRemotely = false;

							FinalObjectFiles.Add(FinalCompiledISPCObjFile);
							Logger.LogDebug("   ISPC Compiling bytecode {StatusDescription}: \"{CommandPath}\" {CommandArguments} {ProducedItems}", PostCompileAction.StatusDescription, PostCompileAction.CommandPath, PostCompileAction.CommandArguments, PostCompileAction.ProducedItems);
						}
						// Override the output object files
						CompiledISPCObjFiles = FinalObjectFiles;
					}
				}

				Result.ObjectFiles.AddRange(CompiledISPCObjFiles);
			}

			return Result;
		}
	}
}
