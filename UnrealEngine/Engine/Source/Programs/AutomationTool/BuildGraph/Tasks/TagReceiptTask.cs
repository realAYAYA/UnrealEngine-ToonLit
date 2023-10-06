// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.BuildGraph;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using EpicGames.Core;
using UnrealBuildBase;
using UnrealBuildTool;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the Tag Receipt task.
	/// </summary>
	public class TagReceiptTaskParameters
	{
		/// <summary>
		/// Set of receipt files (*.target) to read, including wildcards and tag names, separated by semicolons.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// Path to the Engine folder, used to expand $(EngineDir) properties in receipt files. Defaults to the Engine directory for the current workspace.
		/// </summary>
		[TaskParameter(Optional = true)]
		public DirectoryReference EngineDir;

		/// <summary>
		/// Path to the project folder, used to expand $(ProjectDir) properties in receipt files. Defaults to the Engine directory for the current workspace -- DEPRECATED.
		/// </summary>
		[TaskParameter(Optional = true)]
		public DirectoryReference ProjectDir;

		/// <summary>
		/// Whether to tag the Build Products listed in receipts.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool BuildProducts;

		/// <summary>
		/// Which type of Build Products to tag (see TargetReceipt.cs - UnrealBuildTool.BuildProductType for valid values).
		/// </summary>
		[TaskParameter(Optional = true)]
		public string BuildProductType;

		/// <summary>
		/// Whether to tag the Runtime Dependencies listed in receipts.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool RuntimeDependencies;

		/// <summary>
		/// Which type of Runtime Dependencies to tag (see TargetReceipt.cs - UnrealBuildTool.StagedFileType for valid values).
		/// </summary>
		[TaskParameter(Optional = true)]
		public string StagedFileType;

		/// <summary>
		/// Name of the tag to apply.
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.TagList)]
		public string With;
	}

	/// <summary>
	/// Task that tags build products and/or runtime dependencies by reading from *.target files.
	/// </summary>
	[TaskElement("TagReceipt", typeof(TagReceiptTaskParameters))]
	class TagReceiptTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters to this task
		/// </summary>
		TagReceiptTaskParameters Parameters;

		/// <summary>
		/// The type of build products to enumerate. May be null.
		/// </summary>
		Nullable<BuildProductType> BuildProductType;

		/// <summary>
		/// The type of staged files to enumerate. May be null,
		/// </summary>
		Nullable<StagedFileType> StagedFileType;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters to select which files to search</param>
		public TagReceiptTask(TagReceiptTaskParameters InParameters)
		{
			Parameters = InParameters;

			if (!String.IsNullOrEmpty(Parameters.BuildProductType))
			{
				BuildProductType = (BuildProductType)Enum.Parse(typeof(BuildProductType), Parameters.BuildProductType);
			}
			if (!String.IsNullOrEmpty(Parameters.StagedFileType))
			{
				StagedFileType = (StagedFileType)Enum.Parse(typeof(StagedFileType), Parameters.StagedFileType);
			}
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override async Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Output a warning if the project directory is specified
			if (Parameters.ProjectDir != null)
			{
				Logger.LogWarning("The ProjectDir argument to the TagReceipt parameter is deprecated. This path is now determined automatically from the receipt.");
			}

			// Set the Engine directory
			DirectoryReference EngineDir = Parameters.EngineDir ?? Unreal.EngineDirectory;

			// Resolve the input list
			IEnumerable<FileReference> TargetFiles = ResolveFilespec(Unreal.RootDirectory, Parameters.Files, TagNameToFileSet);

			// Filter the files
			HashSet<FileReference> Files = await ExecuteAsync(EngineDir, TargetFiles, Parameters.BuildProducts, BuildProductType, Parameters.RuntimeDependencies, StagedFileType);

			// Apply the tag to all the matching files
			FindOrAddTagSet(TagNameToFileSet, Parameters.With).UnionWith(Files);
		}

		public static Task<HashSet<FileReference>> ExecuteAsync(DirectoryReference EngineDir, IEnumerable<FileReference> TargetFiles, bool BuildProducts, BuildProductType? BuildProductType, bool RuntimeDependencies, StagedFileType? StagedFileType = null)
		{
			HashSet<FileReference> Files = new HashSet<FileReference>();

			foreach (FileReference TargetFile in TargetFiles)
			{
				// check all files are .target files
				if (TargetFile.GetExtension() != ".target")
				{
					throw new AutomationException("Invalid file passed to TagReceipt task ({0})", TargetFile.FullName);
				}

				// Read the receipt
				TargetReceipt Receipt;
				if (!TargetReceipt.TryRead(TargetFile, EngineDir, out Receipt))
				{
					Logger.LogWarning("Unable to load file using TagReceipt task ({Arg0})", TargetFile.FullName);
					continue;
				}

				if (BuildProducts)
				{
					foreach (BuildProduct BuildProduct in Receipt.BuildProducts)
					{
						if(BuildProductType.HasValue && BuildProduct.Type != BuildProductType.Value)
						{
							continue;
						}
						if(StagedFileType.HasValue && TargetReceipt.GetStageTypeFromBuildProductType(BuildProduct) != StagedFileType.Value)
						{
							continue;
						}
						Files.Add(BuildProduct.Path);
					}
				}

				if (RuntimeDependencies)
				{
					foreach (RuntimeDependency RuntimeDependency in Receipt.RuntimeDependencies)
					{
						// Skip anything that doesn't match the files we want
						if(BuildProductType.HasValue)
						{
							continue;
						}
						if(StagedFileType.HasValue && RuntimeDependency.Type != StagedFileType.Value)
						{
							continue;
						}

						// Check which files exist, and warn about any that don't. Ignore debug files, as they are frequently excluded for size (eg. UE on GitHub). This matches logic during staging.
						FileReference DependencyPath = RuntimeDependency.Path;
						if (FileReference.Exists(DependencyPath))
						{
							Files.Add(DependencyPath);
						}
						else if(RuntimeDependency.Type != UnrealBuildTool.StagedFileType.DebugNonUFS)
						{
							Logger.LogWarning("File listed as RuntimeDependency in {Arg0} does not exist ({Arg1})", TargetFile.FullName, DependencyPath.FullName);
						}
					}
				}
			}

			return Task.FromResult(Files);
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are required by this task
		/// </summary>
		/// <returns>The tag names which are required by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			return FindTagNamesFromFilespec(Parameters.Files);
		}

		/// <summary>
		/// Find all the referenced tags from tasks in this task
		/// </summary>
		/// <returns>The tag names which are produced/modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			return FindTagNamesFromList(Parameters.With);
		}
	}

	/// <summary>
	/// Extension methods
	/// </summary>
	public static class TaskExtensions
	{
		/// <summary>
		/// Task that tags build products and/or runtime dependencies by reading from *.target files.
		/// </summary>
		public static async Task<FileSet> TagReceiptsAsync(this FileSet Files, DirectoryReference EngineDir = null, bool BuildProducts = false, BuildProductType? BuildProductType = null, bool RuntimeDependencies = false, StagedFileType? StagedFileType = null)
		{
			HashSet<FileReference> Result = await TagReceiptTask.ExecuteAsync(EngineDir ?? Unreal.EngineDirectory, Files, BuildProducts, BuildProductType, RuntimeDependencies, StagedFileType);
			return FileSet.FromFiles(Unreal.RootDirectory, Result);
		}
	}
}
