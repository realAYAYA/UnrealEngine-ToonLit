// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a copy task
	/// </summary>
	public class CopyTaskParameters
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
		/// Whether or not to overwrite existing files.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool Overwrite = true;

		/// <summary>
		/// Tag to be applied to build products of this task.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;

		/// <summary>
		/// Whether or not to throw an error if no files were found to copy
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool ErrorIfNotFound = false;
	}

	/// <summary>
	/// Copies files from one directory to another.
	/// </summary>
	[TaskElement("Copy", typeof(CopyTaskParameters))]
	public class CopyTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		CopyTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public CopyTask(CopyTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override async Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Parse all the source patterns
			FilePattern SourcePattern = new FilePattern(Unreal.RootDirectory, Parameters.From);

			// Parse the target pattern
			FilePattern TargetPattern = new FilePattern(Unreal.RootDirectory, Parameters.To);

			// Apply the filter to the source files
			HashSet<FileReference> Files = null;
			if (!String.IsNullOrEmpty(Parameters.Files))
			{
				SourcePattern = SourcePattern.AsDirectoryPattern();
				Files = ResolveFilespec(SourcePattern.BaseDirectory, Parameters.Files, TagNameToFileSet);
			}

			// Build the file mapping
			Dictionary<FileReference, FileReference> TargetFileToSourceFile = FilePattern.CreateMapping(Files, ref SourcePattern, ref TargetPattern);

			// Check we got some files
			if (TargetFileToSourceFile.Count == 0)
			{
				if (Parameters.ErrorIfNotFound)
				{
					Logger.LogError("No files found matching '{SourcePattern}'", SourcePattern);
				}
				else
				{
					Logger.LogInformation("No files found matching '{SourcePattern}'", SourcePattern);
				}
				return;
			}

			// Run the copy
			Logger.LogInformation("Copying {Arg0} file{Arg1} from {Arg2} to {Arg3}...", TargetFileToSourceFile.Count, (TargetFileToSourceFile.Count == 1) ? "" : "s", SourcePattern.BaseDirectory, TargetPattern.BaseDirectory);
			await ExecuteAsync(TargetFileToSourceFile, Parameters.Overwrite);

			// Update the list of build products
			BuildProducts.UnionWith(TargetFileToSourceFile.Keys);

			// Apply the optional output tag to them
			foreach (string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(TargetFileToSourceFile.Keys);
			}
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="TargetFileToSourceFile"></param>
		/// <param name="Overwrite"></param>
		/// <returns></returns>
		public static Task ExecuteAsync(Dictionary<FileReference, FileReference> TargetFileToSourceFile, bool Overwrite)
		{
			//  If we're not overwriting, remove any files where the destination file already exists.
			if (!Overwrite)
			{
				Dictionary<FileReference, FileReference> FilteredTargetToSourceFile = new Dictionary<FileReference, FileReference>();
				foreach (KeyValuePair<FileReference, FileReference> File in TargetFileToSourceFile)
				{
					if (FileReference.Exists(File.Key))
					{
						Logger.LogInformation("Not copying existing file {Arg0}", File.Key);
						continue;
					}
					FilteredTargetToSourceFile.Add(File.Key, File.Value);
				}
				if(FilteredTargetToSourceFile.Count == 0)
				{
					Logger.LogWarning("All files already exist, exiting early.");
					return Task.CompletedTask;
				}
				TargetFileToSourceFile = FilteredTargetToSourceFile;
			}

			// If the target is on a network share, retry creating the first directory until it succeeds
			DirectoryReference FirstTargetDirectory = TargetFileToSourceFile.First().Key.Directory;
			if(!DirectoryReference.Exists(FirstTargetDirectory))
			{
				const int MaxNumRetries = 15;
				for(int NumRetries = 0;;NumRetries++)
				{
					try
					{
						DirectoryReference.CreateDirectory(FirstTargetDirectory);
						if(NumRetries == 1)
						{
							Logger.LogInformation("Created target directory {FirstTargetDirectory} after 1 retry.", FirstTargetDirectory);
						}
						else if(NumRetries > 1)
						{
							Logger.LogInformation("Created target directory {FirstTargetDirectory} after {NumRetries} retries.", FirstTargetDirectory, NumRetries);
						}
						break;
					}
					catch(Exception Ex)
					{
						if(NumRetries == 0)
						{
							Logger.LogInformation("Unable to create directory '{FirstTargetDirectory}' on first attempt. Retrying {MaxNumRetries} times...", FirstTargetDirectory, MaxNumRetries);
						}

						Logger.LogDebug("  {Ex}", Ex);

						if(NumRetries >= 15)
						{
							throw new AutomationException(Ex, "Unable to create target directory '{0}' after {1} retries.", FirstTargetDirectory, NumRetries);
						}

						Thread.Sleep(2000);
					}
				}
			}

			// Copy them all
			KeyValuePair<FileReference, FileReference>[] FilePairs = TargetFileToSourceFile.ToArray();
			foreach(KeyValuePair<FileReference, FileReference> FilePair in FilePairs)
			{
				Logger.LogDebug("  {Arg0} -> {Arg1}", FilePair.Value, FilePair.Key);
			}
			CommandUtils.ThreadedCopyFiles(FilePairs.Select(x => x.Value.FullName).ToList(), FilePairs.Select(x => x.Key.FullName).ToList(), bQuiet: true, bRetry: true);
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

	/// <summary>
	/// Extension methods
	/// </summary>
	public static partial class BgStateExtensions
	{
		/// <summary>
		/// Copy files from one location to another
		/// </summary>
		/// <param name="Files">The files to copy</param>
		/// <param name="TargetDir"></param>
		/// <param name="Overwrite">Whether or not to overwrite existing files.</param>
		public static async Task<FileSet> CopyToAsync(this FileSet Files, DirectoryReference TargetDir, bool? Overwrite = null)
		{
			// Run the copy
			Dictionary<FileReference, FileReference> TargetFileToSourceFile = Files.Flatten(TargetDir);
			if (TargetFileToSourceFile.Count == 0)
			{
				return FileSet.Empty;
			}

			Log.Logger.LogInformation("Copying {NumFiles} file(s) to {TargetDir}...", TargetFileToSourceFile.Count, TargetDir);
			await CopyTask.ExecuteAsync(TargetFileToSourceFile, Overwrite ?? true);
			return FileSet.FromFiles(TargetFileToSourceFile.Keys.Select(x => (x.MakeRelativeTo(TargetDir), x)));
		}
	}
}
