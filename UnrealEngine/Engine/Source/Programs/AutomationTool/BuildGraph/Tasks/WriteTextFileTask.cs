// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml;
using EpicGames.Core;
using UnrealBuildBase;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a <see cref="WriteTextFileTask"/>.
	/// </summary>
	public class WriteTextFileTaskParameters
	{
		/// <summary>
		/// Path to the file to write.
		/// </summary>
		[TaskParameter]
		public FileReference File;

		/// <summary>
		/// Optional, whether or not to append to the file rather than overwrite.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Append;

		/// <summary>
		/// The text to write to the file.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Text;

		/// <summary>
		/// If specified, causes the given list of files to be printed after the given message.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Writes text to a file.
	/// </summary>
	[TaskElement("WriteTextFile", typeof(WriteTextFileTaskParameters))]
	public class WriteTextFileTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task.
		/// </summary>
		WriteTextFileTaskParameters Parameters;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InParameters">Parameters for this task.</param>
		public WriteTextFileTask(WriteTextFileTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job.</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include.</param>
		public override async Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			string FileText = Parameters.Text;

			// If any files or tagsets are provided, add them to the text output.
			if (!String.IsNullOrEmpty(Parameters.Files))
			{
				if (!string.IsNullOrWhiteSpace(FileText))
				{
					FileText += Environment.NewLine;
				}

				HashSet<FileReference> Files = ResolveFilespec(Unreal.RootDirectory, Parameters.Files, TagNameToFileSet);
				if (Files.Any())
				{
					FileText += string.Join(Environment.NewLine, Files.Select(f => f.FullName));
				}
			}

			// Make sure output folder exists.
			if (!DirectoryReference.Exists(Parameters.File.Directory))
			{
				DirectoryReference.CreateDirectory(Parameters.File.Directory);
			}

			if (Parameters.Append)
			{
				Logger.LogInformation("{Text}", string.Format("Appending text to file '{0}': {1}", Parameters.File, FileText));
				await FileReference.AppendAllTextAsync(Parameters.File, Environment.NewLine + FileText);
			}
			else
			{
				Logger.LogInformation("{Text}", string.Format("Writing text to file '{0}': {1}", Parameters.File, FileText));
				await FileReference.WriteAllTextAsync(Parameters.File, FileText);
			}

			// Apply the optional tag to the build products
			foreach (string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).Add(Parameters.File);
			}

			// Add them to the set of build products
			BuildProducts.Add(Parameters.File);
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
