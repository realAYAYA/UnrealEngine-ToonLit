// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using EpicGames.Jupiter;
using Microsoft.Extensions.Logging;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a zip task
	/// </summary>
	public class UploadTaskParameters
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
		/// The jupiter namespace used to upload the build. Used to control who has access to the build.
		/// </summary>
		[TaskParameter]
		public string JupiterNamespace;

		/// <summary>
		/// The key of the build as will be used to download the build again. This has to be globally unique for this particular upload.
		/// </summary>
		[TaskParameter]
		public string JupiterKey;

		/// <summary>
		/// The type of archive these files are from, will be added to the metadata
		/// </summary>
		[TaskParameter]
		public string ArchiveType;

		/// <summary>
		/// The name of the project this set of files are associated with, will be added to the metadata
		/// </summary>
		[TaskParameter]
		public string ProjectName;

		/// <summary>
		/// The source control branch these files were generated from, will be added to the metadata
		/// </summary>
		[TaskParameter]
		public string Branch;

		/// <summary>
		/// The source control revision these files were generated from, will be added to the metadata
		/// </summary>
		[TaskParameter]
		public string Changelist;

		/// <summary>
		/// Specify the url to the Jupiter instance to upload to
		/// </summary>
		[TaskParameter]
		public string JupiterUrl;

		/// <summary>
		/// Semi-colon separated list of '=' separated key value mappings to add to the metadata. E.g. Foo=bar;spam=eggs
		/// </summary>
		[TaskParameter(Optional = true)]
		public string AdditionalMetadata;

		/// <summary>
		/// If enabled file content is not kept in memory, results in lower memory usage but increased io as file contents needs to be read multiple times (for hashing as well as during upload)
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool LimitMemoryUsage = true;
	}

	/// <summary>
	/// Uploads a set of files to Jupiter for future retrival
	/// </summary>
	[TaskElement("Upload", typeof(UploadTaskParameters))]
	public class UploadTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		UploadTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters for this task</param>
		public UploadTask(UploadTaskParameters InParameters)
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

			// Create the jupiter tree
			Logger.LogInformation("Uploading {NumFiles} files to {Url}...", Files.Count, Parameters.JupiterUrl);

			JupiterFileTree FileTree = new JupiterFileTree(Parameters.FromDir, Parameters.LimitMemoryUsage);
			foreach (FileReference File in Files)
			{
				FileTree.AddFile(File);
			}

			Dictionary<string, object> Metadata = new Dictionary<string, object>
			{
				{"ArchiveType", Parameters.ArchiveType},
				{"Project", Parameters.ProjectName},
				{"Branch", Parameters.Branch},
				{"Changelist", Parameters.Changelist},
			};

			if (Parameters.AdditionalMetadata != null)
			{
				string[] KV = Parameters.AdditionalMetadata.Split(';');
				foreach (string Option in KV)
				{
					int SeparatorIndex = Option.IndexOf('=');
					if (SeparatorIndex == -1)
						continue;

					string Key = Option.Substring(0, SeparatorIndex);
					string Value = Option.Substring(SeparatorIndex + 1);

					Metadata[Key] = Value;
				}
			}
			// Upload the tree to Jupiter
			Dictionary<FileReference, List<string>> Mapping = await FileTree.UploadToJupiter(Parameters.JupiterUrl, Parameters.JupiterNamespace, Parameters.JupiterKey, Metadata);

			// Debug output of which files mapped to which blobs, can be useful to determine which files are constantly being uploaded
			// Json.Save(FileReference.Combine(AutomationTool.Unreal.RootDirectory, "JupiterUpload.json"), Mapping);
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
		/// Finds the tags which are produced by this task
		/// </summary>
		/// <returns>The tag names which are produced by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return new string[] { };
		}
	}
}
