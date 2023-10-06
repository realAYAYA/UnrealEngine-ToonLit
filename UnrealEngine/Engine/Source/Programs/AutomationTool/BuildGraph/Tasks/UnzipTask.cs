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
	/// Parameters for a zip task
	/// </summary>
	public class UnzipTaskParameters
	{
		/// <summary>
		/// Path to the zip file to extract.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string ZipFile;

		/// <summary>
		/// Output directory for the extracted files.
		/// </summary>
		[TaskParameter]
		public DirectoryReference ToDir;

		/// <summary>
		/// Whether or not to use the legacy unzip code.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool UseLegacyUnzip = false;

		/// <summary>
		/// Whether or not to overwrite files during unzip.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool OverwriteFiles = true;

		/// <summary>
		/// Tag to be applied to the extracted files.
		/// </summary>
		[TaskParameter(Optional = true, ValidationType = TaskParameterValidationType.TagList)]
		public string Tag;
	}

	/// <summary>
	/// Extract files from a zip archive.
	/// </summary>
	[TaskElement("Unzip", typeof(UnzipTaskParameters))]
	public class UnzipTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		UnzipTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public UnzipTask(UnzipTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names \to the set of files they include</param>
		public override Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			DirectoryReference ToDir = Parameters.ToDir;

			// Find all the zip files
			IEnumerable<FileReference> ZipFiles = ResolveFilespec(Unreal.RootDirectory, Parameters.ZipFile, TagNameToFileSet);

			// Extract the files
			HashSet<FileReference> OutputFiles = new HashSet<FileReference>();
			foreach(FileReference ZipFile in ZipFiles)
			{
				if (Parameters.UseLegacyUnzip)
				{
					OutputFiles.UnionWith(CommandUtils.LegacyUnzipFiles(ZipFile.FullName, ToDir.FullName, Parameters.OverwriteFiles).Select(x => new FileReference(x)));
				}
				else
				{
					OutputFiles.UnionWith(CommandUtils.UnzipFiles(ZipFile, ToDir, Parameters.OverwriteFiles));
				}
			}

			// Apply the optional tag to the produced archive
			foreach(string TagName in FindTagNamesFromList(Parameters.Tag))
			{
				FindOrAddTagSet(TagNameToFileSet, TagName).UnionWith(OutputFiles);
			}

			// Add the archive to the set of build products
			BuildProducts.UnionWith(OutputFiles);
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
			return FindTagNamesFromFilespec(Parameters.ZipFile);
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
