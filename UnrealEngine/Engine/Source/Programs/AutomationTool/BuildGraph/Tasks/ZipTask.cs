// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;
using Microsoft.Extensions.Logging;
using System.IO;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a zip task
	/// </summary>
	public class ZipTaskParameters
	{
		/// <summary>
		/// The directory to read compressed files from.
		/// </summary>
		[TaskParameter]
		public DirectoryReference FromDir;

		/// <summary>
		/// List of file specifications separated by semicolons (for example, *.cpp;Engine/.../*.bat), or the name of a tag set. Relative paths are taken from FromDir.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// List of files that should have an executable bit set.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.FileSpec)]
		public string ExecutableFiles;

		/// <summary>
		/// The zip file to create.
		/// </summary>
		[TaskParameter]
		public FileReference ZipFile;

		/// <summary>
		/// Tag to be applied to the created zip file.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Compresses files into a zip archive.
	/// </summary>
	[TaskElement("Zip", typeof(ZipTaskParameters))]
	public class ZipTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		ZipTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public ZipTask(ZipTaskParameters InParameters)
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
			// Find all the input files
			List<FileReference> Files;
			if(Parameters.Files == null)
			{
				Files = DirectoryReference.EnumerateFiles(Parameters.FromDir, "*", System.IO.SearchOption.AllDirectories).ToList();
			}
			else
			{
				Files = ResolveFilespec(Parameters.FromDir, Parameters.Files, TagNameToFileSet).ToList();
			}

			// Create the zip file
			Logger.LogInformation("Adding {NumFiles} files to {ZipFile}...", Files.Count, Parameters.ZipFile);

			HashSet<FileReference> ExecutableFiles = null;
			if (Parameters.ExecutableFiles != null)
			{
				ExecutableFiles = ResolveFilespec(Parameters.FromDir, Parameters.ExecutableFiles, TagNameToFileSet);
				foreach (FileReference ExecutableFile in Files.Intersect(ExecutableFiles))
				{
					Logger.LogInformation("  Executable file: {File}", ExecutableFile);
				}
			}

			CommandUtils.ZipFiles(Parameters.ZipFile, Parameters.FromDir, Files, ExecutableFiles);

			// Apply the optional tag to the produced archive
			foreach (string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).Add(Parameters.ZipFile);
			}

			// Add the archive to the set of build products
			BuildProducts.Add(Parameters.ZipFile);
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
}
