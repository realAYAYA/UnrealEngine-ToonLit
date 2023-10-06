// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
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
    /// Parameters for a task that uploads symbols to a symbol server
    /// </summary>
    public class SymStoreTaskParameters
    {
        /// <summary>
        /// The platform toolchain required to handle symbol files.
        /// </summary>
        [TaskParameter]
        public UnrealTargetPlatform Platform;

        /// <summary>
        /// List of output files. PDBs will be extracted from this list.
        /// </summary>
        [TaskParameter]
        public string Files;

        /// <summary>
        /// Output directory for the compressed symbols.
        /// </summary>
        [TaskParameter]
        public string StoreDir;

        /// <summary>
        /// Name of the product for the symbol store records.
        /// </summary>
        [TaskParameter]
        public string Product;

		/// <summary>
		/// Name of the Branch to base all the depot source files from.
		/// Used when IndexSources is true (may be used only on some platforms).
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Branch;

		/// <summary>
		/// Changelist to which all the depot source files have been synced to.
		/// Used when IndexSources is true (may be used only on some platforms).
		/// </summary>
		[TaskParameter(Optional = true)]
		public int Change;

		/// <summary>
		/// BuildVersion associated with these symbols. Used for clean-up in AgeStore by matching this version against a directory name in a build share.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string BuildVersion;

		/// <summary>
		/// Whether to include the source code index in the uploaded symbols.
		/// When enabled, the task will generate data required by a source server (only some platforms and source control servers are supported).
		/// The source server allows debuggers to automatically fetch the matching source code when debbugging builds or analyzing dumps.
		/// </summary>
		[TaskParameter(Optional = true)]
		public bool IndexSources = false;

		/// <summary>
		/// Filter for the depot source files that are to be indexed.
		/// It's a semicolon-separated list of perforce filter e.g. Engine/....cpp;Engine/....h.
		/// It may also be a name of a previously defined tag e.g. "#SourceFiles
		/// Used when IndexSources is true (may be used only on some platforms).
		/// </summary>
		[TaskParameter(Optional = true)]
		public string SourceFiles;
	}

    /// <summary>
    /// Task that strips symbols from a set of files.
    /// </summary>
    [TaskElement("SymStore", typeof(SymStoreTaskParameters))]
    public class SymStoreTask : BgTaskImpl
    {
        /// <summary>
        /// Parameters for this task
        /// </summary>
        SymStoreTaskParameters Parameters;

        /// <summary>
        /// Construct a spawn task
        /// </summary>
        /// <param name="InParameters">Parameters for the task</param>
        public SymStoreTask(SymStoreTaskParameters InParameters)
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
			// Find the matching files
			List<FileReference> Files = ResolveFilespec(Unreal.RootDirectory, Parameters.Files, TagNameToFileSet).ToList();
            
            // Get the symbol store directory
            DirectoryReference StoreDir = ResolveDirectory(Parameters.StoreDir);

			// Take the lock before accessing the symbol server, if required by the platform
			Platform TargetPlatform = Platform.GetPlatform(Parameters.Platform);

			List<FileReference> SourceFiles = new List<FileReference>();

			if (Parameters.IndexSources && TargetPlatform.SymbolServerSourceIndexingRequiresListOfSourceFiles)
			{
				Logger.LogInformation("Discovering source code files...");

				SourceFiles = ResolveFilespec(Unreal.RootDirectory, Parameters.SourceFiles, TagNameToFileSet).ToList();
			}

			CommandUtils.OptionallyTakeLock(TargetPlatform.SymbolServerRequiresLock, StoreDir, TimeSpan.FromMinutes(60), () =>
			{
				if (!TargetPlatform.PublishSymbols(StoreDir, Files, Parameters.IndexSources, SourceFiles,
					Parameters.Product, Parameters.Branch, Parameters.Change, Parameters.BuildVersion))
				{
					throw new AutomationException("Failure publishing symbol files.");
				}
			});

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
            yield break;
        }
    }
}
