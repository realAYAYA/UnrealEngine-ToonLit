// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using AutomationTool.Tasks;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a task that strips symbols from a set of files
	/// </summary>
	public class StripTaskParameters
	{
		/// <summary>
		/// The platform toolchain to strip binaries.
		/// </summary>
		[TaskParameter]
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// The directory to find files in.
		/// </summary>
		[TaskParameter(Optional = true)]
		public DirectoryReference BaseDir;

		/// <summary>
		/// List of file specifications separated by semicolons (for example, Engine/.../*.pdb), or the name of a tag set.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// Output directory for the stripped files. Defaults to the input path, overwriting the input files.
		/// </summary>
		[TaskParameter(Optional = true)]
		public DirectoryReference OutputDir;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Strips debugging information from a set of files.
	/// </summary>
	[TaskElement("Strip", typeof(StripTaskParameters))]
	public class StripTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		StripTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public StripTask(StripTaskParameters InParameters)
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
			// Get the base directory
			DirectoryReference BaseDir = Parameters.BaseDir;

			// Get the output directory
			DirectoryReference OutputDir = Parameters.OutputDir;

			// Find the matching files
			FileReference[] SourceFiles = ResolveFilespec(BaseDir, Parameters.Files, TagNameToFileSet).OrderBy(x => x.FullName).ToArray();

			// Create the matching target files
			FileReference[] TargetFiles = SourceFiles.Select(x => FileReference.Combine(OutputDir, x.MakeRelativeTo(BaseDir))).ToArray();

			// Run the stripping command
			Platform TargetPlatform = Platform.GetPlatform(Parameters.Platform);
			for (int Idx = 0; Idx < SourceFiles.Length; Idx++)
			{
				DirectoryReference.CreateDirectory(TargetFiles[Idx].Directory);
				if (SourceFiles[Idx] == TargetFiles[Idx])
				{
					Logger.LogInformation("Stripping symbols: {Arg0}", SourceFiles[Idx].FullName);
				}
				else
				{
					Logger.LogInformation("Stripping symbols: {Arg0} -> {Arg1}", SourceFiles[Idx].FullName, TargetFiles[Idx].FullName);
				}
				TargetPlatform.StripSymbols(SourceFiles[Idx], TargetFiles[Idx]);
			}

			// Apply the optional tag to the build products
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(TargetFiles);
			}

			// Add the target files to the set of build products
			BuildProducts.UnionWith(TargetFiles);
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
			return FindTagNamesFromFilespec(Parameters.Files);
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return FindTagNamesFromList(Parameters.Tag);
		}
	}

	public static partial class StandardTasks
	{
		/// <summary>
		/// Strips symbols from a set of files.
		/// </summary>
		/// <param name="Files"></param>
		/// <param name="Platform">The platform toolchain to strip binaries.</param>
		/// <param name="BaseDir">The directory to find files in.</param>
		/// <param name="OutputDir">Output directory for the stripped files. Defaults to the input path, overwriting the input files.</param>
		/// <returns></returns>
		public static async Task<FileSet> StripAsync(FileSet Files, UnrealTargetPlatform Platform, DirectoryReference BaseDir = null, DirectoryReference OutputDir = null)
		{
			StripTaskParameters Parameters = new StripTaskParameters();
			Parameters.Platform = Platform;
			Parameters.BaseDir = BaseDir;
			Parameters.Files = String.Join(";", Files.Flatten().Values.Select(x => x.FullName));
			Parameters.OutputDir = OutputDir;
			return await ExecuteAsync(new StripTask(Parameters));
		}
	}
}
