// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;
using UnrealBuildBase;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a move task
	/// </summary>
	public class MoveTaskParameters
	{
		/// <summary>
		/// Optional filter to be applied to the list of input files.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// The pattern(s) to copy from (for example, Engine/*.txt).
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string From;

		/// <summary>
		/// The directory to copy to.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string To;

		/// <summary>
		/// Optionally if files should be overwritten, defaults to false.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Overwrite = false;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Moves files from one directory to another.
	/// </summary>
	[TaskElement("Move", typeof(MoveTaskParameters))]
	public class MoveTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		MoveTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public MoveTask(MoveTaskParameters InParameters)
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
			// Parse all the source patterns
			FilePattern SourcePattern = new FilePattern(Unreal.RootDirectory, Parameters.From);

			// Parse the target pattern
			FilePattern TargetPattern = new FilePattern(Unreal.RootDirectory, Parameters.To);

			// Apply the filter to the source files
			HashSet<FileReference> Files = null;
			if(!String.IsNullOrEmpty(Parameters.Files))
			{
				SourcePattern = SourcePattern.AsDirectoryPattern();
				Files = ResolveFilespec(SourcePattern.BaseDirectory, Parameters.Files, TagNameToFileSet);
			}

			// Build the file mapping
			Dictionary<FileReference, FileReference> TargetFileToSourceFile = FilePattern.CreateMapping(Files, ref SourcePattern, ref TargetPattern);

			// Check we got some files
			if(TargetFileToSourceFile.Count == 0)
			{
				CommandUtils.LogInformation("No files found matching '{0}'", SourcePattern);
				return Task.CompletedTask;
			}

			// Copy them all
			CommandUtils.LogInformation("Moving {0} file{1} from {2} to {3}...", TargetFileToSourceFile.Count, (TargetFileToSourceFile.Count == 1)? "" : "s", SourcePattern.BaseDirectory, TargetPattern.BaseDirectory);
			CommandUtils.ParallelMoveFiles(TargetFileToSourceFile.Select(x => new KeyValuePair<FileReference, FileReference>(x.Value, x.Key)), Parameters.Overwrite);

			// Update the list of build products
			BuildProducts.UnionWith(TargetFileToSourceFile.Keys);

			// Apply the optional output tag to them
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(TargetFileToSourceFile.Keys);
			}

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
			foreach(string TagName in FindTagNamesFromFilespec(Parameters.Files))
			{
				yield return TagName;
			}
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
}
