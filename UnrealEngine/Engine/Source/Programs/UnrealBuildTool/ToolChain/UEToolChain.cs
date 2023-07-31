// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	abstract class UEToolChain
	{
		protected readonly ILogger Logger;

		public UEToolChain(ILogger InLogger)
		{
			Logger = InLogger;
		}

		public virtual void SetEnvironmentVariables()
		{
		}

		public virtual void GetVersionInfo(List<string> Lines)
		{
		}

		public virtual void GetExternalDependencies(HashSet<FileItem> ExternalDependencies)
		{
		}

		public static DirectoryReference GetModuleInterfaceDir(DirectoryReference OutputDir)
		{
			return DirectoryReference.Combine(OutputDir, "Ifc");
		}

		public abstract CPPOutput CompileCPPFiles(CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference OutputDir, string ModuleName, IActionGraphBuilder Graph);

		public virtual CPPOutput CompileRCFiles(CppCompileEnvironment Environment, List<FileItem> InputFiles, DirectoryReference OutputDir, IActionGraphBuilder Graph)
		{
			CPPOutput Result = new CPPOutput();
			return Result;
		}

		public virtual CPPOutput CompileISPCFiles(CppCompileEnvironment Environment, List<FileItem> InputFiles, DirectoryReference OutputDir, IActionGraphBuilder Graph)
		{
			CPPOutput Result = new CPPOutput();
			return Result;
		}

		public virtual CPPOutput GenerateISPCHeaders(CppCompileEnvironment Environment, List<FileItem> InputFiles, DirectoryReference OutputDir, IActionGraphBuilder Graph)
		{
			CPPOutput Result = new CPPOutput();
			return Result;
		}

		public virtual void GenerateTypeLibraryHeader(CppCompileEnvironment CompileEnvironment, ModuleRules.TypeLibrary TypeLibrary, FileReference OutputFile, IActionGraphBuilder Graph)
		{
			throw new NotSupportedException("This platform does not support type libraries.");
		}

		public abstract FileItem? LinkFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph);
		public virtual FileItem[] LinkAllFiles(LinkEnvironment LinkEnvironment, bool bBuildImportLibraryOnly, IActionGraphBuilder Graph)
		{
			FileItem? LinkFile = LinkFiles(LinkEnvironment, bBuildImportLibraryOnly, Graph);
			return LinkFile != null ? new FileItem[] { LinkFile } : new FileItem[] { };
		}

		/// <summary>
		/// Get the name of the response file for the current linker environment and output file
		/// </summary>
		/// <param name="LinkEnvironment"></param>
		/// <param name="OutputFile"></param>
		/// <returns></returns>
		public static FileReference GetResponseFileName(LinkEnvironment LinkEnvironment, FileItem OutputFile)
		{
			// Construct a relative path for the intermediate response file
			return FileReference.Combine(LinkEnvironment.IntermediateDirectory!, OutputFile.Location.GetFileName() + ".response");
		}

		public virtual ICollection<FileItem> PostBuild(FileItem Executable, LinkEnvironment ExecutableLinkEnvironment, IActionGraphBuilder Graph)
		{
			return new List<FileItem>();
		}

		public virtual void SetUpGlobalEnvironment(ReadOnlyTargetRules Target)
		{
		}

		public virtual void ModifyBuildProducts(ReadOnlyTargetRules Target, UEBuildBinary Binary, List<string> Libraries, List<UEBuildBundleResource> BundleResources, Dictionary<FileReference, BuildProductType> BuildProducts)
		{
		}

		public virtual void ModifyTargetReceipt(TargetReceipt Receipt)
		{
		}

		public virtual void FinalizeOutput(ReadOnlyTargetRules Target, TargetMakefileBuilder MakefileBuilder)
		{
		}

		public virtual void PrepareRuntimeDependencies(List<RuntimeDependency> RuntimeDependencies, Dictionary<FileReference, FileReference> TargetFileToSourceFile, DirectoryReference ExeDir)
		{
		}

		/// <summary>
		/// Adds a build product and its associated debug file to a receipt.
		/// </summary>
		/// <param name="OutputFile">Build product to add</param>
		/// <param name="OutputType">The type of build product</param>
		public virtual bool ShouldAddDebugFileToReceipt(FileReference OutputFile, BuildProductType OutputType)
		{
			return true;
		}

		public virtual FileReference GetDebugFile(FileReference OutputFile, string DebugExtension)
		{
			//  by default, just change the extension to the debug extension
			return OutputFile.ChangeExtension(DebugExtension);
		}

		public virtual void SetupBundleDependencies(List<UEBuildBinary> Binaries, string GameName)
		{
		}

		public virtual string GetSDKVersion()
		{
			return "Not Applicable";
		}

		/// <summary>
		/// Runs the provided tool and argument. Returns the output, using a rexex capture if one is provided
		/// </summary>
		/// <param name="Command">Full path to the tool to run</param>
		/// <param name="ToolArg">Argument that will be passed to the tool</param>
		/// <param name="Expression">null, or a Regular expression to capture in the output</param>
		/// <returns></returns>
		protected string? RunToolAndCaptureOutput(FileReference Command, string ToolArg, string? Expression = null)
		{
			string ProcessOutput = Utils.RunLocalProcessAndReturnStdOut(Command.FullName, ToolArg, Logger);

			if (string.IsNullOrEmpty(Expression))
			{
				return ProcessOutput;
			}

			Match M = Regex.Match(ProcessOutput, Expression);
			return M.Success ? M.Groups[1].ToString() : null;
		}

		/// <summary>
		/// Runs the provided tool and argument and parses the output to retrieve the version
		/// </summary>
		/// <param name="Command">Full path to the tool to run</param>
		/// <param name="VersionArg">Argument that will result in the version string being shown (it's ok this is a byproduct of a command that returns an error)</param>
		/// <param name="VersionExpression">Regular expression to capture the version. By default we look for four integers separated by periods, with the last two optional</param>
		/// <returns></returns>
		public Version RunToolAndCaptureVersion(FileReference Command, string VersionArg, string VersionExpression = @"(\d+\.\d+(\.\d+)?(\.\d+)?)")
		{
			string? ProcessOutput = RunToolAndCaptureOutput(Command, VersionArg, VersionExpression);

			Version? ToolVersion;

			if (Version.TryParse(ProcessOutput, out ToolVersion))
			{
				return ToolVersion;
			}

			Logger.LogWarning("Unable to retrieve version from {Command} {Arg}", Command, VersionArg);

			return new Version(0, 0);
		}
	};
}
