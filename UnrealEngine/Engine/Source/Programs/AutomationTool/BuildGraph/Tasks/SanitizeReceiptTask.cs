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

using static AutomationTool.CommandUtils;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for the Tag Receipt task.
	/// </summary>
	public class SanitizeReceiptTaskParameters
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
	}

	/// <summary>
	/// Task that tags build products and/or runtime dependencies by reading from *.target files.
	/// </summary>
	[TaskElement("SanitizeReceipt", typeof(SanitizeReceiptTaskParameters))]
	class SanitizeReceiptTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters to this task
		/// </summary>
		SanitizeReceiptTaskParameters Parameters;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InParameters">Parameters to select which files to search</param>
		public SanitizeReceiptTask(SanitizeReceiptTaskParameters InParameters)
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
			// Set the Engine directory
			DirectoryReference EngineDir = Parameters.EngineDir ?? Unreal.EngineDirectory;

			// Resolve the input list
			IEnumerable<FileReference> TargetFiles = ResolveFilespec(Unreal.RootDirectory, Parameters.Files, TagNameToFileSet);
			await ExecuteAsync(TargetFiles, EngineDir);
		}

		public static Task ExecuteAsync(IEnumerable<FileReference> TargetFiles, DirectoryReference EngineDir)
		{
			EngineDir ??= Unreal.EngineDirectory;

			foreach(FileReference TargetFile in TargetFiles)
			{
				// check all files are .target files
				if (TargetFile.GetExtension() != ".target")
				{
					throw new AutomationException("Invalid file passed to TagReceipt task ({0})", TargetFile.FullName);
				}

				// Print the name of the file being scanned
				Logger.LogInformation("Sanitizing {TargetFile}", TargetFile);
				using(new LogIndentScope("  "))
				{
					// Read the receipt
					TargetReceipt Receipt;
					if (!TargetReceipt.TryRead(TargetFile, EngineDir, out Receipt))
					{
						Logger.LogWarning("Unable to load file using TagReceipt task ({Arg0})", TargetFile.FullName);
						continue;
					}

					// Remove any build products that don't exist
					List<BuildProduct> NewBuildProducts = new List<BuildProduct>(Receipt.BuildProducts.Count);
					foreach(BuildProduct BuildProduct in Receipt.BuildProducts)
					{
						if(FileReference.Exists(BuildProduct.Path))
						{
							NewBuildProducts.Add(BuildProduct);
						}
						else
						{
							Logger.LogInformation("Removing build product: {File}", BuildProduct.Path);
						}
					}
					Receipt.BuildProducts = NewBuildProducts;

					// Remove any runtime dependencies that don't exist
					RuntimeDependencyList NewRuntimeDependencies = new RuntimeDependencyList();
					foreach(RuntimeDependency RuntimeDependency in Receipt.RuntimeDependencies)
					{
						if(FileReference.Exists(RuntimeDependency.Path))
						{
							NewRuntimeDependencies.Add(RuntimeDependency);
						}
						else
						{
							Logger.LogInformation("Removing runtime dependency: {File}", RuntimeDependency.Path);
						}
					}
					Receipt.RuntimeDependencies = NewRuntimeDependencies;
				
					// Save the new receipt
					Receipt.Write(TargetFile, EngineDir);
				}
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
			return new string[0];
		}
	}

	/// <summary>
	/// Extension methods
	/// </summary>
	public static class SanitizeReceiptExtensions
	{
		/// <summary>
		/// Sanitize the given receipt files, removing any files that don't exist in the current workspace
		/// </summary>
		public static async Task SanitizeReceiptsAsync(this FileSet TargetFiles, DirectoryReference EngineDir = null)
		{
			await SanitizeReceiptTask.ExecuteAsync(TargetFiles, EngineDir);
		}
	}
}
