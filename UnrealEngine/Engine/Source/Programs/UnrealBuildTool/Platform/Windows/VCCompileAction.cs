// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// Interface for an action that compiles C++ source code
	/// </summary>
	interface ICppCompileAction : IExternalAction
	{
		/// <summary>
		/// Path to the compiled module interface file
		/// </summary>
		FileItem? CompiledModuleInterfaceFile { get; }
	}

	/// <summary>
	/// Serializer which creates a portable object file and allows caching it
	/// </summary>
	class VCCompileAction : ICppCompileAction
	{
		/// <summary>
		/// The action type
		/// </summary>
		public ActionType ActionType { get; set; } = ActionType.Compile;

		/// <summary>
		/// Artifact support for this step
		/// </summary>
		public ArtifactMode ArtifactMode { get; set; } = ArtifactMode.None;

		/// <summary>
		/// Path to the compiler
		/// </summary>
		public FileItem CompilerExe { get; }

		/// <summary>
		/// The type of compiler being used
		/// </summary>
		public WindowsCompiler CompilerType { get; }

		/// <summary>
		/// The version of the compiler being used
		/// </summary>
		public string ToolChainVersion { get; }

		/// <summary>
		/// Source file to compile
		/// </summary>
		public FileItem? SourceFile { get; set; }

		/// <summary>
		/// The object file to output
		/// </summary>
		public FileItem? ObjectFile { get; set; }

		/// <summary>
		/// The assembly file to output
		/// </summary>
		public FileItem? AssemblyFile { get; set; }

		/// <summary>
		/// The output preprocessed file
		/// </summary>
		public FileItem? PreprocessedFile { get; set; }

		/// <summary>
		/// The output analyze warning and error log file
		/// </summary>
		public FileItem? AnalyzeLogFile { get; set; }

		/// <summary>
		/// The output experimental warning and error log file
		/// </summary>
		public FileItem? ExperimentalLogFile { get; set; }

		/// <summary>
		/// The dependency list file
		/// </summary>
		public FileItem? DependencyListFile { get; set; }

		/// <summary>
		/// Compiled module interface
		/// </summary>
		public FileItem? CompiledModuleInterfaceFile { get; set; }

		/// <summary>
		/// For C++ source files, specifies a timing file used to track timing information.
		/// </summary>
		public FileItem? TimingFile { get; set; }

		/// <summary>
		/// Response file for the compiler
		/// </summary>
		public FileItem? ResponseFile { get; set; }

		/// <summary>
		/// The precompiled header file
		/// </summary>
		public FileItem? CreatePchFile { get; set; }

		/// <summary>
		/// The precompiled header file
		/// </summary>
		public FileItem? UsingPchFile { get; set; }

		/// <summary>
		/// The header which matches the PCH
		/// </summary>
		public FileItem? PchThroughHeaderFile { get; set; }

		/// <summary>
		/// List of include paths
		/// </summary>
		public List<DirectoryReference> IncludePaths { get; } = new List<DirectoryReference>();

		/// <summary>
		/// List of system include paths
		/// </summary>
		public List<DirectoryReference> SystemIncludePaths { get; } = new List<DirectoryReference>();

		/// <summary>
		/// List of macro definitions
		/// </summary>
		public List<string> Definitions { get; } = new List<string>();

		/// <summary>
		/// List of force included files
		/// </summary>
		public List<FileItem> ForceIncludeFiles = new List<FileItem>();

		/// <summary>
		/// Every file this action depends on.  These files need to exist and be up to date in order for this action to even be considered
		/// </summary>
		public List<FileItem> AdditionalPrerequisiteItems { get; } = new List<FileItem>();

		/// <summary>
		/// The files that this action produces after completing
		/// </summary>
		public List<FileItem> AdditionalProducedItems { get; } = new List<FileItem>();

		/// <summary>
		/// Arguments to pass to the compiler
		/// </summary>
		public List<string> Arguments { get; } = new List<string>();

		/// <summary>
		/// Whether to show included files to the console
		/// </summary>
		public bool bShowIncludes { get; set; }

		/// <summary>
		/// Whether to override the normal logic for UsingClFilter and force it on.
		/// </summary>
		public bool ForceClFilter = false;

		/// <summary>
		/// Architecture this is compiling for (used for display)
		/// </summary>
		public UnrealArch Architecture { get; set; }

		/// <summary>
		/// Whether this compile is static code analysis (used for display)
		/// </summary>
		public bool bIsAnalyzing { get; set; }

		#region Public IAction implementation

		/// <summary>
		/// Items that should be deleted before running this action
		/// </summary>
		public List<FileItem> DeleteItems { get; } = new List<FileItem>();

		/// <inheritdoc/>
		public bool bCanExecuteRemotely { get; set; }

		/// <inheritdoc/>
		public bool bCanExecuteRemotelyWithSNDBS { get; set; }

		/// <inheritdoc/>
		public bool bCanExecuteRemotelyWithXGE { get; set; } = true;

		/// <inheritdoc/>
		public bool bCanExecuteInUBA { get; set; } = true;

		/// <inheritdoc/>
		public bool bUseActionHistory => true;

		/// <inheritdoc/>
		public bool bIsHighPriority => CreatePchFile != null;

		/// <inheritdoc/>
		public double Weight { get; set; } = 1.0;
		#endregion

		#region Implementation of IAction

		IEnumerable<FileItem> IExternalAction.DeleteItems => DeleteItems;
		public DirectoryReference WorkingDirectory => Unreal.EngineSourceDirectory;
		string IExternalAction.CommandDescription
		{
			get
			{
				if (PreprocessedFile != null)
				{
					return $"Preprocess [{Architecture}]";
				}
				else if (bIsAnalyzing)
				{
					return $"Analyze [{Architecture}]";
				}
				return $"Compile [{Architecture}]";
			}
		}
		bool IExternalAction.bIsGCCCompiler => false;
		bool IExternalAction.bProducesImportLibrary => false;
		string IExternalAction.StatusDescription => (SourceFile == null) ? "Compiling" : SourceFile.Location.GetFileName();
		bool IExternalAction.bShouldOutputStatusDescription => CompilerType.IsClang();

		/// <inheritdoc/>
		IEnumerable<FileItem> IExternalAction.PrerequisiteItems
		{
			get
			{
				if (ResponseFile != null)
				{
					yield return ResponseFile;
				}
				if (SourceFile != null)
				{
					yield return SourceFile;
				}
				if (UsingPchFile != null)
				{
					yield return UsingPchFile;
				}
				foreach (FileItem AdditionalPrerequisiteItem in AdditionalPrerequisiteItems)
				{
					yield return AdditionalPrerequisiteItem;
				}
			}
		}

		/// <inheritdoc/>
		IEnumerable<FileItem> IExternalAction.ProducedItems
		{
			get
			{
				if (ObjectFile != null)
				{
					yield return ObjectFile;
				}
				if (AssemblyFile != null)
				{
					yield return AssemblyFile;
				}
				if (PreprocessedFile != null)
				{
					yield return PreprocessedFile;
				}
				if (AnalyzeLogFile != null)
				{
					yield return AnalyzeLogFile;
				}
				if (ExperimentalLogFile != null)
				{
					yield return ExperimentalLogFile;
				}
				if (DependencyListFile != null)
				{
					yield return DependencyListFile;
				}
				if (TimingFile != null)
				{
					yield return TimingFile;
				}
				if (CreatePchFile != null)
				{
					yield return CreatePchFile;
				}
				foreach (FileItem AdditionalProducedItem in AdditionalProducedItems)
				{
					yield return AdditionalProducedItem;
				}
			}
		}

		/// <summary>
		/// Whether to use cl-filter
		/// </summary>
		bool UsingClFilter => ForceClFilter || (DependencyListFile != null && !DependencyListFile.HasExtension(".json") && !DependencyListFile.HasExtension(".d"));

		/// <inheritdoc/>
		FileReference IExternalAction.CommandPath
		{
			get
			{
				if (UsingClFilter)
				{
					return FileReference.Combine(Unreal.EngineDirectory, "Build", "Windows", "cl-filter", "cl-filter.exe");
				}
				else
				{
					return CompilerExe.Location;
				}
			}
		}

		/// <inheritdoc/>
		string IExternalAction.CommandArguments
		{
			get
			{
				if (UsingClFilter)
				{
					return GetClFilterArguments();
				}
				else
				{
					return GetClArguments();
				}
			}
		}

		/// <inheritdoc/>
		string IExternalAction.CommandVersion => ToolChainVersion;

		#endregion

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Environment">Compiler executable</param>
		public VCCompileAction(VCEnvironment Environment)
		{
			CompilerExe = FileItem.GetItemByFileReference(Environment.CompilerPath);
			CompilerType = Environment.Compiler;
			ToolChainVersion = Environment.ToolChainVersion.ToString();
		}

		/// <summary>
		/// Copy constructor
		/// </summary>
		/// <param name="InAction">Action to copy from</param>
		public VCCompileAction(VCCompileAction InAction)
		{
			ActionType = InAction.ActionType;
			ArtifactMode = InAction.ArtifactMode;
			CompilerExe = InAction.CompilerExe;
			CompilerType = InAction.CompilerType;
			ToolChainVersion = InAction.ToolChainVersion;
			SourceFile = InAction.SourceFile;
			ObjectFile = InAction.ObjectFile;
			AssemblyFile = InAction.AssemblyFile;
			PreprocessedFile = InAction.PreprocessedFile;
			AnalyzeLogFile = InAction.AnalyzeLogFile;
			ExperimentalLogFile = InAction.ExperimentalLogFile;
			DependencyListFile = InAction.DependencyListFile;
			CompiledModuleInterfaceFile = InAction.CompiledModuleInterfaceFile;
			TimingFile = InAction.TimingFile;
			ResponseFile = InAction.ResponseFile;
			CreatePchFile = InAction.CreatePchFile;
			UsingPchFile = InAction.UsingPchFile;
			PchThroughHeaderFile = InAction.PchThroughHeaderFile;
			IncludePaths = new List<DirectoryReference>(InAction.IncludePaths);
			SystemIncludePaths = new List<DirectoryReference>(InAction.SystemIncludePaths);
			Definitions = new List<string>(InAction.Definitions);
			ForceIncludeFiles = new List<FileItem>(InAction.ForceIncludeFiles);
			Arguments = new List<string>(InAction.Arguments);
			bShowIncludes = InAction.bShowIncludes;
			bCanExecuteRemotely = InAction.bCanExecuteRemotely;
			bCanExecuteRemotelyWithSNDBS = InAction.bCanExecuteRemotelyWithSNDBS;
			bCanExecuteRemotelyWithXGE = InAction.bCanExecuteRemotelyWithXGE;
			Architecture = InAction.Architecture;
			bIsAnalyzing = InAction.bIsAnalyzing;
			Weight = InAction.Weight;

			AdditionalPrerequisiteItems = new List<FileItem>(InAction.AdditionalPrerequisiteItems);
			AdditionalProducedItems = new List<FileItem>(InAction.AdditionalProducedItems);
			DeleteItems = new List<FileItem>(InAction.DeleteItems);
		}

		/// <summary>
		/// Serialize a cache handler from an archive
		/// </summary>
		/// <param name="Reader">Reader to serialize from</param>
		public VCCompileAction(BinaryArchiveReader Reader)
		{
			ActionType = (ActionType)Reader.ReadInt();
			ArtifactMode = (ArtifactMode)Reader.ReadByte();
			CompilerExe = Reader.ReadFileItem()!;
			CompilerType = (WindowsCompiler)Reader.ReadInt();
			ToolChainVersion = Reader.ReadString()!;
			SourceFile = Reader.ReadFileItem();
			ObjectFile = Reader.ReadFileItem();
			AssemblyFile = Reader.ReadFileItem();
			PreprocessedFile = Reader.ReadFileItem();
			AnalyzeLogFile = Reader.ReadFileItem();
			ExperimentalLogFile = Reader.ReadFileItem();
			DependencyListFile = Reader.ReadFileItem();
			CompiledModuleInterfaceFile = Reader.ReadFileItem();
			TimingFile = Reader.ReadFileItem();
			ResponseFile = Reader.ReadFileItem();
			CreatePchFile = Reader.ReadFileItem();
			UsingPchFile = Reader.ReadFileItem();
			PchThroughHeaderFile = Reader.ReadFileItem();
			IncludePaths = Reader.ReadList(() => Reader.ReadCompactDirectoryReference())!;
			SystemIncludePaths = Reader.ReadList(() => Reader.ReadCompactDirectoryReference())!;
			Definitions = Reader.ReadList(() => Reader.ReadString())!;
			ForceIncludeFiles = Reader.ReadList(() => Reader.ReadFileItem())!;
			Arguments = Reader.ReadList(() => Reader.ReadString())!;
			bShowIncludes = Reader.ReadBool();
			bCanExecuteRemotely = Reader.ReadBool();
			bCanExecuteRemotelyWithSNDBS = Reader.ReadBool();
			bCanExecuteRemotelyWithXGE = Reader.ReadBool();
			bCanExecuteInUBA = Reader.ReadBool();
			Architecture = UnrealArch.Parse(Reader.ReadString()!);
			bIsAnalyzing = Reader.ReadBool();
			Weight = Reader.ReadDouble();

			AdditionalPrerequisiteItems = Reader.ReadList(() => Reader.ReadFileItem())!;
			AdditionalProducedItems = Reader.ReadList(() => Reader.ReadFileItem())!;
			DeleteItems = Reader.ReadList(() => Reader.ReadFileItem())!;
		}

		/// <inheritdoc/>
		public void Write(BinaryArchiveWriter Writer)
		{
			Writer.WriteInt((int)ActionType);
			Writer.WriteByte((byte)ArtifactMode);
			Writer.WriteFileItem(CompilerExe);
			Writer.WriteInt((int)CompilerType);
			Writer.WriteString(ToolChainVersion);
			Writer.WriteFileItem(SourceFile);
			Writer.WriteFileItem(ObjectFile);
			Writer.WriteFileItem(AssemblyFile);
			Writer.WriteFileItem(PreprocessedFile);
			Writer.WriteFileItem(AnalyzeLogFile);
			Writer.WriteFileItem(ExperimentalLogFile);
			Writer.WriteFileItem(DependencyListFile);
			Writer.WriteFileItem(CompiledModuleInterfaceFile);
			Writer.WriteFileItem(TimingFile);
			Writer.WriteFileItem(ResponseFile);
			Writer.WriteFileItem(CreatePchFile);
			Writer.WriteFileItem(UsingPchFile);
			Writer.WriteFileItem(PchThroughHeaderFile);
			Writer.WriteList(IncludePaths, Item => Writer.WriteCompactDirectoryReference(Item));
			Writer.WriteList(SystemIncludePaths, Item => Writer.WriteCompactDirectoryReference(Item));
			Writer.WriteList(Definitions, Item => Writer.WriteString(Item));
			Writer.WriteList(ForceIncludeFiles, Item => Writer.WriteFileItem(Item));
			Writer.WriteList(Arguments, Item => Writer.WriteString(Item));
			Writer.WriteBool(bShowIncludes);
			Writer.WriteBool(bCanExecuteRemotely);
			Writer.WriteBool(bCanExecuteRemotelyWithSNDBS);
			Writer.WriteBool(bCanExecuteRemotelyWithXGE);
			Writer.WriteBool(bCanExecuteInUBA);
			Writer.WriteString(Architecture.ToString());
			Writer.WriteBool(bIsAnalyzing);
			Writer.WriteDouble(Weight);

			Writer.WriteList(AdditionalPrerequisiteItems, Item => Writer.WriteFileItem(Item));
			Writer.WriteList(AdditionalProducedItems, Item => Writer.WriteFileItem(Item));
			Writer.WriteList(DeleteItems, Item => Writer.WriteFileItem(Item));
		}

		/// <summary>
		/// Writes the response file with the action's arguments
		/// </summary>
		/// <param name="Graph">The graph builder</param>
		/// <param name="Logger">Logger for output</param>
		public void WriteResponseFile(IActionGraphBuilder Graph, ILogger Logger)
		{
			if (ResponseFile != null)
			{
				Graph.CreateIntermediateTextFile(ResponseFile, GetCompilerArguments(Logger));
			}
		}

		public List<string> GetCompilerArguments(ILogger Logger)
		{
			List<string> Arguments = new List<string>();
			if (SourceFile != null)
			{
				VCToolChain.AddSourceFile(Arguments, SourceFile);
			}

			foreach (DirectoryReference IncludePath in IncludePaths)
			{
				VCToolChain.AddIncludePath(Arguments, IncludePath, CompilerType);
			}

			foreach (DirectoryReference SystemIncludePath in SystemIncludePaths)
			{
				VCToolChain.AddSystemIncludePath(Arguments, SystemIncludePath, CompilerType);
			}

			foreach (string Definition in Definitions)
			{
				// Escape all quotation marks so that they get properly passed with the command line.
				string DefinitionArgument = Definition.Contains('"') ? Definition.Replace("\"", "\\\"") : Definition;
				VCToolChain.AddDefinition(Arguments, DefinitionArgument);
			}

			foreach (FileItem ForceIncludeFile in ForceIncludeFiles)
			{
				VCToolChain.AddForceIncludeFile(Arguments, ForceIncludeFile);
			}

			if (CreatePchFile != null)
			{
				VCToolChain.AddCreatePchFile(Arguments, PchThroughHeaderFile!, CreatePchFile);
			}

			if (UsingPchFile != null && CompilerType.IsMSVC())
			{
				VCToolChain.AddUsingPchFile(Arguments, PchThroughHeaderFile!, UsingPchFile);
			}

			if (PreprocessedFile != null)
			{
				VCToolChain.AddPreprocessedFile(Arguments, PreprocessedFile, Logger);
			}

			if (ObjectFile != null)
			{
				VCToolChain.AddObjectFile(Arguments, ObjectFile);
			}

			if (AssemblyFile != null)
			{
				VCToolChain.AddAssemblyFile(Arguments, AssemblyFile);
			}

			if (AnalyzeLogFile != null)
			{
				VCToolChain.AddAnalyzeLogFile(Arguments, AnalyzeLogFile);
			}

			if (ExperimentalLogFile != null)
			{
				VCToolChain.AddExperimentalLogFile(Arguments, ExperimentalLogFile);
			}

			// A better way to express this? .json is used as output for /sourceDependencies), but .md.json is used as output for /sourceDependencies:directives)
			if (DependencyListFile != null && DependencyListFile.HasExtension(".json") && !DependencyListFile.HasExtension(".md.json"))
			{
				VCToolChain.AddSourceDependenciesFile(Arguments, DependencyListFile);
			}

			if (DependencyListFile != null && DependencyListFile.HasExtension(".d"))
			{
				VCToolChain.AddSourceDependsFile(Arguments, DependencyListFile);
			}

			Arguments.AddRange(this.Arguments);
			return Arguments;
		}

		string GetClArguments()
		{
			if (ResponseFile == null)
			{
				return String.Join(" ", Arguments);
			}
			else
			{
				string ResponseFileString = VCToolChain.NormalizeCommandLinePath(ResponseFile);

				// cl.exe can't handle response files with a path longer than 260 characters, and relative paths can push it over the limit
				if (!System.IO.Path.IsPathRooted(ResponseFileString) && System.IO.Path.Combine(WorkingDirectory.FullName, ResponseFileString).Length > 260)
				{
					ResponseFileString = ResponseFile.FullName;
				}
				return String.Format("@{0}", Utils.MakePathSafeToUseWithCommandLine(ResponseFileString));
			}
		}

		string GetClFilterArguments()
		{
			List<string> Arguments = new List<string>();
			string DependencyListFileString = VCToolChain.NormalizeCommandLinePath(DependencyListFile!);
			Arguments.Add(String.Format("-dependencies={0}", Utils.MakePathSafeToUseWithCommandLine(DependencyListFileString)));

			if (TimingFile != null)
			{
				string TimingFileString = VCToolChain.NormalizeCommandLinePath(TimingFile);
				Arguments.Add(String.Format("-timing={0}", Utils.MakePathSafeToUseWithCommandLine(TimingFileString)));
			}
			if (bShowIncludes)
			{
				Arguments.Add("-showincludes");
			}

			Arguments.Add(String.Format("-compiler={0}", Utils.MakePathSafeToUseWithCommandLine(CompilerExe.AbsolutePath)));
			Arguments.Add("--");
			Arguments.Add(Utils.MakePathSafeToUseWithCommandLine(CompilerExe.AbsolutePath));
			Arguments.Add(GetClArguments());
			Arguments.Add("/showIncludes");

			return String.Join(" ", Arguments);
		}
	}

	/// <summary>
	/// Serializer for <see cref="VCCompileAction"/> instances
	/// </summary>
	class VCCompileActionSerializer : ActionSerializerBase<VCCompileAction>
	{
		/// <inheritdoc/>
		public override VCCompileAction Read(BinaryArchiveReader Reader)
		{
			return new VCCompileAction(Reader);
		}

		/// <inheritdoc/>
		public override void Write(BinaryArchiveWriter Writer, VCCompileAction Action)
		{
			Action.Write(Writer);
		}
	}
}
