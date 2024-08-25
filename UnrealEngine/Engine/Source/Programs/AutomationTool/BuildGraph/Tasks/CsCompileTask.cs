// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildBase;
using UnrealBuildTool;
using Microsoft.Extensions.Logging;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a task that compiles a C# project
	/// </summary>
	public class CsCompileTaskParameters
	{
		/// <summary>
		/// The C# project file to compile. Using semicolons, more than one project file can be specified.
		/// </summary>
		[TaskParameter]
		public string Project;

		/// <summary>
		/// The configuration to compile.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Configuration;

		/// <summary>
		/// The platform to compile.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Platform;

		/// <summary>
		/// The target to build.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Target;

		/// <summary>
		/// Properties for the command
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Properties;

		/// <summary>
		/// Additional options to pass to the compiler.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Arguments;

		/// <summary>
		/// Only enumerate build products -- do not actually compile the projects.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool EnumerateOnly;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;

		/// <summary>
		/// Tag to be applied to any non-private references the projects have.
		/// (for example, those that are external and not copied into the output directory).
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string TagReferences;

		/// <summary>
		/// Whether to use the system toolchain rather than the bundled UE SDK
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool UseSystemCompiler;
	}

	/// <summary>
	/// Compiles C# project files, and their dependencies.
	/// </summary>
	[TaskElement("CsCompile", typeof(CsCompileTaskParameters))]
	public class CsCompileTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for the task
		/// </summary>
		CsCompileTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CsCompileTask(CsCompileTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Get the project file
			HashSet<FileReference> ProjectFiles = ResolveFilespec(Unreal.RootDirectory, Parameters.Project, TagNameToFileSet);
			foreach(FileReference ProjectFile in ProjectFiles)
			{
				if(!FileReference.Exists(ProjectFile))
				{
					throw new AutomationException("Couldn't find project file '{0}'", ProjectFile.FullName);
				}
				if(!ProjectFile.HasExtension(".csproj"))
				{
					throw new AutomationException("File '{0}' is not a C# project", ProjectFile.FullName);
				}
			}

			// Get the default properties
			Dictionary<string, string> Properties = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);
			if(!String.IsNullOrEmpty(Parameters.Platform))
			{
				Properties["Platform"] = Parameters.Platform;
			}
			if(!String.IsNullOrEmpty(Parameters.Configuration))
			{
				Properties["Configuration"] = Parameters.Configuration;
			}
			if (!String.IsNullOrEmpty(Parameters.Properties))
			{
				foreach (string Property in Parameters.Properties.Split(';'))
				{
					if (!String.IsNullOrWhiteSpace(Property))
					{
						int EqualsIdx = Property.IndexOf('=');
						if (EqualsIdx == -1)
						{
							Logger.LogWarning("Missing '=' in property assignment");
						}
						else
						{
							Properties[Property.Substring(0, EqualsIdx).Trim()] = Property.Substring(EqualsIdx + 1).Trim();
						}
					}
				}
			}

			// Build the arguments and run the build
			if(!Parameters.EnumerateOnly)
			{
				List<string> Arguments = new List<string>();
				foreach(KeyValuePair<string, string> PropertyPair in Properties)
				{
					Arguments.Add(String.Format("/property:{0}={1}", CommandUtils.MakePathSafeToUseWithCommandLine(PropertyPair.Key), CommandUtils.MakePathSafeToUseWithCommandLine(PropertyPair.Value)));
				}
				if(!String.IsNullOrEmpty(Parameters.Arguments))
				{
					Arguments.Add(Parameters.Arguments);
				}
				if(!String.IsNullOrEmpty(Parameters.Target))
				{
					Arguments.Add(String.Format("/target:{0}", CommandUtils.MakePathSafeToUseWithCommandLine(Parameters.Target)));
				}

				Arguments.Add("/restore");
				Arguments.Add("/verbosity:minimal");
				Arguments.Add("/nologo");

				string JoinedArguments = String.Join(" ", Arguments);

				foreach(FileReference ProjectFile in ProjectFiles)
				{
					if (!FileReference.Exists(ProjectFile))
					{
						throw new AutomationException("Project {0} does not exist!", ProjectFile);
					}

					if (Parameters.UseSystemCompiler)
					{
						CommandUtils.MsBuild(CommandUtils.CmdEnv, ProjectFile.FullName, JoinedArguments, null);
					}
					else
					{
						CommandUtils.RunAndLog(CommandUtils.CmdEnv, CommandUtils.CmdEnv.DotnetMsbuildPath, $"msbuild {CommandUtils.MakePathSafeToUseWithCommandLine(ProjectFile.FullName)} {JoinedArguments}");
					}
				}
			}

			// Try to figure out the output files
			HashSet<FileReference> ProjectBuildProducts;
			HashSet<FileReference> ProjectReferences;
			FindBuildProductsAndReferences(ProjectFiles, Properties, out ProjectBuildProducts, out ProjectReferences);

			// Apply the optional tag to the produced archive
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(ProjectBuildProducts);
			}

			// Apply the optional tag to any references
			if (!String.IsNullOrEmpty(Parameters.TagReferences))
			{
				foreach (string TagName in FindTagNamesFromList(Parameters.TagReferences))
				{
					FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(ProjectReferences);
				}
			}

			// Merge them into the standard set of build products
			BuildProducts.UnionWith(ProjectBuildProducts);
			BuildProducts.UnionWith(ProjectReferences);
			return Task.CompletedTask;
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			return FindTagNamesFromFilespec(Parameters.Project);
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			foreach (string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				yield return TagName;
			}

			foreach (string TagName in FindTagNamesFromList(Parameters.TagReferences))
			{
				yield return TagName;
			}
		}

		/// <summary>
		/// Find all the build products created by compiling the given project file
		/// </summary>
		/// <param name="ProjectFiles">Initial project file to read. All referenced projects will also be read.</param>
		/// <param name="InitialProperties">Mapping of property name to value</param>
		/// <param name="OutBuildProducts">Receives a set of build products on success</param>
		/// <param name="OutReferences">Receives a set of non-private references on success</param>
		static void FindBuildProductsAndReferences(HashSet<FileReference> ProjectFiles, Dictionary<string, string> InitialProperties, out HashSet<FileReference> OutBuildProducts, out HashSet<FileReference> OutReferences)
		{
			// Find all the build products and references
			OutBuildProducts = new HashSet<FileReference>();
			OutReferences = new HashSet<FileReference>();

			// Read all the project information into a dictionary
			Dictionary<FileReference, CsProjectInfo> FileToProjectInfo = new Dictionary<FileReference, CsProjectInfo>();
			foreach(FileReference ProjectFile in ProjectFiles)
			{
				// Read all the projects
				ReadProjectsRecursively(ProjectFile, InitialProperties, FileToProjectInfo);

				// Find all the outputs for each project
				foreach(KeyValuePair<FileReference, CsProjectInfo> Pair in FileToProjectInfo)
				{
					CsProjectInfo ProjectInfo = Pair.Value;

					// Add all the build projects from this project
					DirectoryReference OutputDir = ProjectInfo.GetOutputDir(Pair.Key.Directory);
					ProjectInfo.FindBuildProducts(OutputDir, OutBuildProducts, FileToProjectInfo);

					// Add any files which are only referenced
					foreach (KeyValuePair<FileReference, bool> Reference in ProjectInfo.References)
					{
						CsProjectInfo.AddReferencedAssemblyAndSupportFiles(Reference.Key, OutReferences);
					}
				}
			}

			OutBuildProducts.RemoveWhere(x => !FileReference.Exists(x));
			OutReferences.RemoveWhere(x => !FileReference.Exists(x));
		}

		/// <summary>
		/// Read a project file, plus all the project files it references.
		/// </summary>
		/// <param name="File">Project file to read</param>
		/// <param name="InitialProperties">Mapping of property name to value for the initial project</param>
		/// <param name="FileToProjectInfo"></param>
		/// <returns>True if the projects were read correctly, false (and prints an error to the log) if not</returns>
		static void ReadProjectsRecursively(FileReference File, Dictionary<string, string> InitialProperties, Dictionary<FileReference, CsProjectInfo> FileToProjectInfo)
		{
			// Early out if we've already read this project
			if (!FileToProjectInfo.ContainsKey(File))
			{
				// Try to read this project
				CsProjectInfo ProjectInfo;
				if (!CsProjectInfo.TryRead(File, InitialProperties, out ProjectInfo))
				{
					throw new AutomationException("Couldn't read project '{0}'", File.FullName);
				}

				// Add it to the project lookup, and try to read all the projects it references
				FileToProjectInfo.Add(File, ProjectInfo);
				foreach(FileReference ProjectReference in ProjectInfo.ProjectReferences.Keys)
				{
					if(!FileReference.Exists(ProjectReference))
					{
						throw new AutomationException("Unable to find project '{0}' referenced by '{1}'", ProjectReference, File);
					}
					ReadProjectsRecursively(ProjectReference, InitialProperties, FileToProjectInfo);
				}
			}
		}
	}

	/// <summary>
	/// Output from compiling a csproj file
	/// </summary>
	public class CsCompileOutput
	{
		/// <summary>
		/// Empty instance of CsCompileOutput
		/// </summary>
		public static CsCompileOutput Empty { get; } = new CsCompileOutput(FileSet.Empty, FileSet.Empty);

		/// <summary>
		/// Output binaries
		/// </summary>
		public FileSet Binaries { get; }

		/// <summary>
		/// Referenced output
		/// </summary>
		public FileSet References { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public CsCompileOutput(FileSet Binaries, FileSet References)
		{
			this.Binaries = Binaries;
			this.References = References;
		}

		/// <summary>
		/// Merge all outputs from this project
		/// </summary>
		/// <returns></returns>
		public FileSet Merge()
		{
			return Binaries + References;
		}

		/// <summary>
		/// Merges two outputs together
		/// </summary>
		public static CsCompileOutput operator +(CsCompileOutput Lhs, CsCompileOutput Rhs)
		{
			return new CsCompileOutput(Lhs.Binaries + Rhs.Binaries, Lhs.References + Rhs.References);
		}
	}

	/// <summary>
	/// Extension methods for csproj compilation
	/// </summary>
	public static class CsCompileOutputExtensions
	{
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Task"></param>
		/// <returns></returns>
		public static async Task<FileSet> MergeAsync(this Task<CsCompileOutput> Task)
		{
			return (await Task).Merge();
		}
	}

	public static partial class StandardTasks
	{
		/// <summary>
		/// Compile a C# project
		/// </summary>
		/// <param name="Project">The C# project files to compile.</param>
		/// <param name="Platform">The platform to compile.</param>
		/// <param name="Configuration">The configuration to compile.</param>
		/// <param name="Target">The target to build.</param>
		/// <param name="Properties">Properties for the command.</param>
		/// <param name="Arguments">Additional options to pass to the compiler.</param>
		/// <param name="EnumerateOnly">Only enumerate build products -- do not actually compile the projects.</param>
		public static async Task<CsCompileOutput> CsCompileAsync(FileReference Project, string Platform = null, string Configuration = null, string Target = null, string Properties = null, string Arguments = null, bool? EnumerateOnly = null)
		{
			CsCompileTaskParameters Parameters = new CsCompileTaskParameters();
			Parameters.Project = Project.FullName;
			Parameters.Platform = Platform;
			Parameters.Configuration = Configuration;
			Parameters.Target = Target;
			Parameters.Properties = Properties;
			Parameters.Arguments = Arguments;
			Parameters.EnumerateOnly = EnumerateOnly ?? Parameters.EnumerateOnly;
			Parameters.Tag = "#Out";
			Parameters.TagReferences = "#Refs";

			HashSet<FileReference> BuildProducts = new HashSet<FileReference>();
			Dictionary<string, HashSet<FileReference>> TagNameToFileSet = new Dictionary<string, HashSet<FileReference>>();
			await new CsCompileTask(Parameters).ExecuteAsync(new JobContext(null!, null!), BuildProducts, TagNameToFileSet);

			FileSet Binaries = FileSet.Empty;
			FileSet References = FileSet.Empty;
			if (TagNameToFileSet.TryGetValue(Parameters.Tag, out HashSet<FileReference> BinaryFiles))
			{
				Binaries = FileSet.FromFiles(Unreal.RootDirectory, BinaryFiles);
			}
			if (TagNameToFileSet.TryGetValue(Parameters.TagReferences, out HashSet<FileReference> ReferenceFiles))
			{
				References = FileSet.FromFiles(Unreal.RootDirectory, ReferenceFiles);
			}

			return new CsCompileOutput(Binaries, References);
		}
	}
}
